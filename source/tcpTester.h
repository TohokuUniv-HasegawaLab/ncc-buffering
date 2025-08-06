//*****************************************************************************
// FILENAME	:	tcpTester.h
// ABSTRACT	:	TCP送信テストツール ヘッダ
// NOTE		:	
//*****************************************************************************
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<stdbool.h>
#include 	<poll.h>
#ifdef __APPLE__
#include    <sys/msg.h>
#include    <sys/ipc.h>
#else
#include	<mqueue.h>
#endif
#include	<limits.h>
#include	<time.h>
#include	<unistd.h>
#include	<getopt.h>
#include	<libgen.h>
#include	<errno.h>
#include	<pthread.h>
#include	<netdb.h>
#include	<signal.h>
#include	<sys/types.h>
#ifdef __APPLE__
#include    <dispatch/dispatch.h>
#else
#include    <sys/timerfd.h>
#endif
#include	<sys/stat.h>
#include	<arpa/inet.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<netinet/tcp.h>
#include	<arpa/inet.h>
#include	<sys/time.h>

//-----------------------------------------------------------------------------
// マクロ定義
//-----------------------------------------------------------------------------
#define	MODE_SEND			(1)													// 送信ノード
#define	MODE_RECV			(2)													// 受信ノード
#define	MODE_RELAY			(3)													// 中継ノード
#define	MODE_QUIT			(99)												// 終了

#define	MSGQ_KEY			"/tcpTester"										// メッセージキューキー名
#define LINE_BUF_MAX		(2048)												// １行の最大長
#define	LINE_ITEM_MAX		(128)												// １行の最大トークン数
#define	ITEM_LENGTH_MAX		(256)												// １トークンの最大長
#define	THREAD_MAX			(1024)												// 最大スレッド数
#define OUTPUT_REC_MAX		(100)												// 最大出力結果バッファレコードサイズ
#define SIZE_REC_MAX		(100000)											// 最大出力結果バッファレコードサイズ
#define TRANS_REC_MAX		(10000)												// 最大転送バッファレコードサイズ

#define	FPATH_MAX			(256)												// パス最大長
#define	FNAME_MAX			(256+32)											// ファイル名最大長

#define RET_OK				(0)													// 正常
#define	RET_NG				(-1)												// 異常

// 結果出力スレッド状態
#define	STS_CALC_STOP		(0)													// 停止中
#define	STS_CALC_WAIT		(1)													// 計測待ち待機中
#define	STS_CALC_START		(2)													// 計測開始要求中
#define	STS_CALC_MEASURING	(3)													// 計測中

// デフォルト値
#define	DEF_PORT_NO			(20000)												// 受信ポート番号(default=20000)
#define	DEF_BUF_SIZE		(128 * 1024)										// バッファサイズ(defalut=128Kbyte)
#define	DEF_SEND_SIZE		(2147483648)										// 送信データサイズ(defalut=2G)
#define	DEF_INPUT_NAME		"input.dat"											// 外部プログラム用入力ファイル名
#define	DEF_OUTPUT_NAME		"output.dat"										// 外部プログラム用出力ファイル名
#define	DEF_RNG_BUF_SIZE	(2 * 1000 * 1000 * 1000)							// 転送用バッファサイズ(2Gbyteに設定)
#define	DEF_TIMEOUT			(30)												// タイムアウト時間
#define	DEF_NOTICE_PORT_NO	(30000)												// 受信完了通知先通信ポート番号(default=30000)

#ifdef __APPLE__
#define MSG_BUF_SIZE        (sizeof(thread_param_t))

#endif
// 画面/ログ 出力マクロ(DSP=画面(stdio),ERR=画面(stderr),EOUT(エラーログ),DBG(デバッグログ)
#define DSP(...) fprintf(stdout, __VA_ARGS__)

#define DBG(__prm,fmt,...) \
{ \
	char __b[1024]; \
	char __fn[512]; \
	if(__prm==NULL){ \
		if(g_dbg_mode==1){ \
			strncpy(__fn, g_result_file, sizeof(__fn)); \
			sprintf(__b,"[D]: DBG "fmt, ##__VA_ARGS__); \
			output_log(__fn, __b ); \
		} \
	}else{ \
		if(((thread_param_t*)__prm)->dbg_mode==1){ \
			strncpy(__fn, ((thread_param_t*)__prm)->result_filename, sizeof(__fn)); \
			sprintf(__b,"[%d]: DBG "fmt, ((thread_param_t*)__prm)->thread_no, ##__VA_ARGS__); \
			output_log(__fn, __b ); \
		} \
	} \
}

#define ERR(__prm,fmt,...) \
{ \
	char __b[1024]; \
	char __fn[512]; \
	if(__prm==NULL){ \
		strncpy(__fn, g_result_file, sizeof(__fn)); \
		sprintf(__b,"[D]: ERROR "fmt, ##__VA_ARGS__); \
	}else{ \
		((thread_param_t*)__prm)->error_flg = 1; \
		strncpy(__fn, ((thread_param_t*)__prm)->result_filename, sizeof(__fn)); \
		sprintf(__b,"[%d]: ERROR "fmt, ((thread_param_t*)__prm)->thread_no, ##__VA_ARGS__); \
	} \
	output_log(__fn, __b ); \
}

#define LOG(__prm,fmt,...) \
{ \
	char __b[1024]; \
	char __fn[512]; \
	if(__prm==NULL){ \
		strncpy(__fn, g_result_file, sizeof(__fn)); \
		sprintf(__b,"[D]: "fmt, ##__VA_ARGS__); \
	}else{ \
		strncpy(__fn, ((thread_param_t*)__prm)->result_filename, sizeof(__fn)); \
		sprintf(__b,"[%d]: "fmt, ((thread_param_t*)__prm)->thread_no, ##__VA_ARGS__); \
	} \
	output_log(__fn, __b ); \
}


//-----------------------------------------------------------------------------
// 構造体定義
//-----------------------------------------------------------------------------
// トークン格納構造体
typedef struct {
	char	data[ITEM_LENGTH_MAX];												// データ格納エリア
} ITEM_ARY_T;

// スレッド情報構造体
typedef struct {
	// スレッド情報
	int					thread_no;												// スレッド番号
	// 起動パラメータ
	int					mode;													// 起動モード(MODE_SEND=送信ノード, MODE_RECV=受信ノード, MODE_RELAY=中継ノード, MODE_QUIT=終了)
	char				ip[32];													// 送信先IPアドレス
	unsigned short		port;													// 通信ポート番号
	char				notice_ip[32];											// 受信完了通知先IPアドレス（受信ノードのみ）
	unsigned short		notice_port;											// 受信完了通知先通信ポート番号(送信ノード, 受信ノードのみ)
	ssize_t				size;													// １フローで送信するデータサイズ
	int					congestion_mode;										// 輻輳モード(0=Linux defaultP, 1=CCP…後で文字列に変更するかも);
	char				congestion_name[32];									// 輻輳モード名( cubic(default), reno, ccp)
	int					app_mode;												// 中継モード(0=アプリケーションレベル, 1=トランスポートレベル)
	ssize_t				buf_size;												// バッファサイズ
	time_t				send_start_time;										// 送信開始時刻
	char				result_filename[256+32];								// 結果出力ファイル名
	char				app_filename[256+32];									// 外部プログラム名
	char				input_filename[256+32];									// 外部プログラム用入力ファイル名
	char				output_filename[256+32];								// 外部プログラム用出力ファイル名
	char				save_filename[256+32];									// 受信データ保存ファイル名
	int					interval;												// 計測周期
	int					auto_port;												// 自動ポート選択
	int					trans_rec_num;											// バッファレコード数
	int					snd_bufsize;											// 送信バッファサイズ
	int					rcv_bufsize;											// 受信バッファサイズ
	int					dbg_mode;												// デバッグモード
	int					dbg_mode2;												// デバッグモード2
	int					timeout;												// タイムアウト時間(s)

	// 内部情報
	_Atomic	ssize_t		recv_total;												// 総受信データサイズ
	_Atomic	ssize_t		send_total;												// 総送信データサイズ
	_Atomic	ssize_t		save_total;												// 保存データサイズ
	int					sock_acc;												// accept socket
	int					sock_svr;												// servcer socket
	int					sock_cli;												// client socket
	ssize_t				ring_bufsize;											// リングバッファサイズ
	char*				buf;													// リングバッファポインタ

	// 結果出力スレッド関連
	int					is_run_calc_s;											// ログ計測スレッド実行中フラグ
	int					is_run_calc_r;											// ログ計測スレッド実行中フラグ
	pthread_t			output_thread_handle_s;									// 結果出力スレッドハンドル(送信計測用)
	pthread_t			output_thread_handle_r;									// 結果出力スレッドハンドル(受信計測用)
    int					s_timer_fd;												// 送信用計測タイマーディスクプリタ
    int					r_timer_fd;												// 受信用計測タイマーディスクプリタ
    int					s_stop_fd;												// 送信計測タイマー停止ディスクプリタ
    int					r_stop_fd;												// 受信計測タイマー停止ディスクプリタ

#ifdef __APPLE__
    dispatch_source_t	s_timer_t;												// 送信用計測タイマー
    dispatch_source_t	r_timer_t;												// 受信用計測タイマー
    dispatch_source_t	s_stop_t;												// 送信計測タイマー停止
    dispatch_source_t	r_stop_t;												// 受信計測タイマー停止
    
    int					s_timer_fd_w;											// 送信用計測タイマーディスクプリタ
    int					r_timer_fd_w;											// 受信用計測タイマーディスクプリタ
    int					s_stop_fd_w;											// 送信計測タイマー停止ディスクプリタ
    int					r_stop_fd_w;											// 受信計測タイマー停止ディスクプリタ

#endif
    
	// データ保存関連
	int					save_r_pos;												// 受信データ buffer read position
	int					save_w_pos;												// 受信データ buffer write position
	bool				is_run_save;											// データ保存スレッド 実行中状態

	// トランスポート関連
	int					trans_r_pos;											// transport buffer read position
	int					trans_w_pos;											// Transport buffer write position
	bool				is_run_trans;											// transport(send) thread 実行中状態

	bool				error_flg;												// エラーフラグ
} thread_param_t;


// スレッド管理テーブル構造体
typedef struct {
	int					used_flg;												// 使用中フラグ
	thread_param_t		param;													// 起動パラメータ
} thread_mng_t;

#ifdef __APPLE__
typedef struct {
    long mtype;
    char mtext[MSG_BUF_SIZE];
} msg_buf_t;

#endif
//-----------------------------------------------------------------------------
// 外部変数
//-----------------------------------------------------------------------------
extern	char			g_prc_name[256];										// 自プロセス名
extern	char			g_prc_path[256];										// カレントパス
extern	char			g_result_file[256+32];									// 結果出力ファイル名(デフォルト用)
extern	int				g_dbg_mode;												// デバッグモード(0=OFF, 1=ON)
extern	int				g_dbg_mode2;											// デバッグモード2(0=OFF, 1=ON)
#ifdef __APPLE__
extern    int			g_que;													// メッセージキュー記述子
#else
extern	mqd_t			g_que;													// メッセージキュー記述子
extern	struct mq_attr	g_mq_attr;												// キュー属性情報
#endif
extern	int				g_thread_no;											// スレッド番号
extern thread_mng_t		g_thread_mngtbl[THREAD_MAX];

//-----------------------------------------------------------------------------
// 外部関数数
//-----------------------------------------------------------------------------
extern	int init( int argc, char** argv );										// 初期処理
extern	void term();															// 終了処理
extern	bool is_dir_filepath( char* filepath );									// ディレクトリ指定有無チェック
extern	bool is_ext_filepath( char* filepath );									// 拡張子指定有無チェック
extern	void deg_dsp_variable(thread_param_t* thread_param_p);					// 内部変数表示(DEBGU用)
extern	int token_analize( char* text, char* sep, ITEM_ARY_T* ary );			// 文字列分解処理
extern	void lntrim(char* str);													// 改行文字NULL置換
extern	int is_proc_running(char* proc_name);									// プロセス起動中確認
extern thread_param_t* regist_thread(thread_param_t* param_p);					// スレッド情報登録委
extern int release_thread(thread_param_t* param_p);								// スレッド情報解放
extern time_t set_start_time( char* str_time );									// 送信開始時刻の通算秒取得
extern void start_wait( time_t t );												// 指定時刻までWait
extern void disp_help(int argc, char** argv);									// ヘルプ表示

extern	int exec_command(char* cmdline, ITEM_ARY_T* ary);						// 外部コマンド実行

extern void output_log(char* filename, char* text);								// 結果ログ出力
extern void output_calc_result(thread_param_t* param_p, long long first_us, long long last_us, long long now_us, ssize_t interval_size, ssize_t total_size, int counter, ssize_t rate, char* text );

extern void* send_thread_output(void *p);
extern void* recv_thread_output(void *p);
extern void calc_stop(thread_param_t* param_p, int kind);

