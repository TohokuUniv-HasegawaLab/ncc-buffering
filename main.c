//*****************************************************************************
// FILENAM	:	main.c
// ABSTRAC	:	メイン処理
// NOTE		:	
//*****************************************************************************
#include	"tcpTester.h"

void* thread_send_node(void* p);												// 送信スレッド
void* thread_recv_node(void* p);												// 受信スレッド
void* thread_relay_node(void* p);												// 中継スレッド
void* thread_send(void* p);														// 転送処理(トランスポートレベル用)
void* thread_save(void* p);														// データ保存スレッド

int send_func(thread_param_t* param_p);											// 送信処理
int recv_func(thread_param_t* param_p);											// 受信処理
int recv_func_with_datasave(thread_param_t* param_p);							// 受信処理(データ保存付き)
int tran_func(thread_param_t* param_p);											// トランスポート処理

int termination_notice_send_func(thread_param_t* param_p);
int termination_notice_recv_func(thread_param_t* param_p);
//*****************************************************************************
// MODULE	:	main1
// ABSTRACT	:	メイン処理
// FUNCTION	:
// ARGUMENTS:	INT    argc   (In) 引数の数
//			:	CHAR** argv   (In) 引数のポインタリスト
// RETURN	:	 0 = 正常終了
//			:	-1 = エラー発生
// NOTE		:
//*****************************************************************************
int main(int argc, char** argv)
{
	thread_param_t	quedata = {};												// スレッドパラメータ
	thread_param_t*	param_p = NULL;												// スレッドパラメータ
	pthread_t		thread[THREAD_MAX] = {};									// スレッドハンドルテーブル
	int				ret;														// 汎用戻り値

	// 初期処理
	if( 0 != init(argc, argv) ){
		term();
		exit(-1);
	}

	// メインループ
	while( 1 ){

		DBG(NULL,"waiting in queue...\n" );

		// スレッドパラメータの取得
#ifdef __APPLE__
        msg_buf_t msg_buf = {};
        msg_buf.mtype = 1L;

        if(-1 == msgrcv(g_que, (char*)&msg_buf, MSG_BUF_SIZE, 0L, 0)){
            ERR(NULL,"msgrcv error (%s) %lu\n", strerror(errno) ,MSG_BUF_SIZE);
            break;
        }
        memcpy(&quedata, msg_buf.mtext, sizeof(thread_param_t));
        DBG(NULL,"deque => quedata.mode=%d\n", quedata.mode );

#else
		if(-1 == mq_receive(g_que, (char*)&quedata, g_mq_attr.mq_msgsize, NULL)){
			ERR(NULL,"mq_receive error (%s)\n", strerror(errno) );
			break;
		}
		DBG(NULL,"deque => quedata.mode=%d\n", quedata.mode );
#endif

		// スレッド情報を登録する
		param_p = regist_thread( &quedata );
		if( param_p == NULL ){
			ERR(param_p, "thread space full\n" );
			break;
		}

		// 送信スレッド起動
		if( param_p->mode == MODE_SEND ){
			ret = pthread_create( &thread[param_p->thread_no], NULL, (void *)thread_send_node, (void *)param_p );
			if (ret != 0) {
				ERR(param_p,"can not create thread: %s", strerror(errno) );
				break;
			}
		}

		// 受信スレッド起動
		else if( param_p->mode == MODE_RECV ){
	        ret = pthread_create( &thread[param_p->thread_no], NULL, (void *)thread_recv_node, (void *)param_p);
			if (ret != 0) {
				ERR(param_p,"can not create thread: %s", strerror(errno) );
			}
		}

		// 中継スレッド起動
		else if( param_p->mode == MODE_RELAY ){
	        ret = pthread_create( &thread[param_p->thread_no], NULL, (void *)thread_relay_node, (void *)param_p );
			if( ret != 0 ) {
				ERR(param_p,"can not create thread: %s", strerror(errno) );
				break;
			}
		}
	}
	DBG(param_p,"------------- end -----------\n");

	// 終了処理
	term();

	return 0;
}

#include <x86intrin.h>    // __rdtsc
// 固定クロック周波数（GHz）を指定（必要なら後で動的測定に変更可能）
#define CPU_FREQ_GHZ 2.10
// 高精度スリープ（TSCベース）
void delay_ns_rdtsc(unsigned long long ns) {
    double cycles_per_ns = CPU_FREQ_GHZ;
    uint64_t wait_cycles = (uint64_t)(ns * cycles_per_ns);

    uint64_t start = __rdtsc();
    while (__rdtsc() - start < wait_cycles);
}


//*****************************************************************************
// MODULE	:	wait_for_socket
// ABSTRACT	:	ソケットの書き込み，読み取り可能になるまで待機する
// FUNCTION	:
// ARGUMENTS:	int sockfd		監視するソケット
// 			:	int timeout_ms	タイムアウト時間(ms)
// 			:	int check_write	送信バッファの監視フラグ
// 			:	int check_read	受信バッファの監視フラグ
// RETURN	:	1 = 書き込み可能 or 読み取り可能
// 			:	0 = タイムアウト or 別のエラー発生
// NOTE		:	ソケットをノンブロッキングモードに変更するに伴い追加
// CREATE	:	2025/03/19 田淵追記
// UPDATE	:   
//*****************************************************************************
bool wait_for_socket(thread_param_t* param_p, int sockfd, int timeout_ms, int check_write, int check_read) {
    struct pollfd fds;
    fds.fd = sockfd;
    fds.events = 0;

    if (check_write) fds.events |= POLLOUT;
    if (check_read)  fds.events |= POLLIN;

	// LOG(param_p, "DEBUG: sockfd=%d, check_write=%d, check_read=%d\n", sockfd, check_write, check_read);

    int ret = poll(&fds, 1, timeout_ms);

	// LOG(param_p, "DEBUG: poll revents=0x%x\n", fds.revents);

    if (ret < 0) {
        ERR(param_p,"poll() failed\n");
        return false;
    }
    
    if (ret == 0) {
        ERR(param_p,"poll() timed out\n");
        return false;
    }

    if (fds.revents & POLLERR) {
        ERR(param_p,"Socket error!\n");
        return false;
    }
    if (fds.revents & POLLHUP) {
        ERR(param_p,"Connection closed by peer!\n");
        return false;
    }
    if (fds.revents & POLLNVAL) {
        ERR(param_p,"Invalid socket!\n");
        return false;
    }

    return true;
}


//*****************************************************************************
// MODULE	:	thread_send_node
// ABSTRACT	:	送信ノード処理
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	NULL固定
// NOTE		:
//*****************************************************************************
void* thread_send_node(void* p)
{
	thread_param_t*	param_p = p;
	void*			status;														// スレッド返却値
	int				ret;														// 汎用戻り値

	// 引数を保持
	DBG(param_p,"[%d]: %s start\n", param_p->thread_no, __func__ );
	DBG(param_p,"[%d]: param_p->ip         = %s\n", param_p->thread_no, param_p->ip );
	DBG(param_p,"[%d]: param_p->port       = %d\n", param_p->thread_no, param_p->port );
	DBG(param_p,"[%d]: param_p->size       = %ld\n", param_p->thread_no, param_p->size );
	DBG(param_p,"[%d]: param_p->buf_size   = %ld\n", param_p->thread_no, param_p->buf_size );
	DBG(param_p,"[%d]: param_p->send_start_time = %ld\n", param_p->thread_no, param_p->send_start_time );

	// 結果出力スレッド起動
	ret = pthread_create( &param_p->output_thread_handle_s, NULL, (void *)send_thread_output, (void *)param_p );
	if (ret != 0) {
		ERR(param_p,"can not create thread: %s", strerror(errno) );
	}
	while( (param_p->is_run_calc_s == STS_CALC_STOP) && (param_p->error_flg == 0) ) usleep(1);



	LOG(param_p,"Send node send start\n");
	// 送信処理
	ret = send_func(param_p);
	
	LOG(param_p,"Send node send end\n");

	if( ret != 0 ){
		//処理継続
	}

	// 結果出力スレッドの終了を待つ
//	pthread_join( param_p->output_thread_handle_s, &status );

//	DBG(param_p,"[%d]: %s end\n", param_p->thread_no, __func__ );
	
	// 結果出力スレッド起動
	ret = pthread_create( &param_p->output_thread_handle_r, NULL, (void *)recv_thread_output, (void *)param_p );
	if (ret != 0) {
		ERR(param_p,"can not create thread: %s", strerror(errno) );
	}
	while( (param_p->is_run_calc_r == STS_CALC_STOP) && (param_p->error_flg == 0 ) ) usleep(1);

	// 受信ノードへからの通知を待つ
	ret = termination_notice_recv_func(param_p);


	// 結果出力スレッドの終了を待つ
	pthread_join( param_p->output_thread_handle_s, &status );

	DBG(param_p,"[%d]: %s end\n", param_p->thread_no, __func__ );
	// 結果出力スレッドの終了を待つ
	pthread_join( param_p->output_thread_handle_r, &status );


	// スレッド情報の解放
	release_thread(param_p);
	pthread_detach(pthread_self());
	pthread_exit(NULL);
}


//*****************************************************************************
// MODULE	:	thread_recv_node
// ABSTRACT	:	受信ノード処理
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	NULL固定
// NOTE		:
//*****************************************************************************
void* thread_recv_node(void* p)
{
	thread_param_t*	param_p = p;
	void*			status;														// スレッド返却値
	int				ret;														// 汎用戻り値

	// 引数を保持
	DBG(param_p,"[%d]: %s start\n", param_p->thread_no, __func__ );
	DBG(param_p,"[%d]: param_p->notice_ip  = %s\n", param_p->thread_no, param_p->notice_ip );
	DBG(param_p,"[%d]: param_p->port       = %d\n", param_p->thread_no, param_p->port );
	DBG(param_p,"[%d]: param_p->size       = %ld\n", param_p->thread_no, param_p->size );
	DBG(param_p,"[%d]: param_p->buf_size   = %ld\n", param_p->thread_no, param_p->buf_size );

	// 結果出力スレッド起動
	ret = pthread_create( &param_p->output_thread_handle_r, NULL, (void *)recv_thread_output, (void *)param_p );
	if (ret != 0) {
		ERR(param_p,"can not create thread: %s", strerror(errno) );
	}
	while( (param_p->is_run_calc_r == STS_CALC_STOP) && (param_p->error_flg == 0 ) ) usleep(1);

	// 受信処理を呼び出す
	if( strlen( param_p->save_filename ) == 0 ){								// 受信データ保存指定されている場合
		ret = recv_func(param_p);
	}else{																		// 受信データ保存指定されていない場合
		ret = recv_func_with_datasave(param_p);
	}
	if( ret != 0 ){
		//処理継続
	}

	// 結果出力スレッドの終了を待つ
	pthread_join( param_p->output_thread_handle_r, &status );

	
	// 結果出力スレッド起動
	ret = pthread_create( &param_p->output_thread_handle_s, NULL, (void *)send_thread_output, (void *)param_p );
	if (ret != 0) {
		ERR(param_p,"can not create thread: %s", strerror(errno) );
	}

	while( (param_p->is_run_calc_s == STS_CALC_STOP) && (param_p->error_flg == 0) ) usleep(1);

	// 送信ノードへ終了通知を送る
	ret = termination_notice_send_func(param_p);


	// 結果出力スレッドの終了を待つ
	pthread_join( param_p->output_thread_handle_s, &status );
	
	DBG(param_p,"[%d]: %s end\n", param_p->thread_no, __func__ );
	
	// スレッド情報の解放
	release_thread(param_p);
	pthread_detach(pthread_self());
	pthread_exit(NULL);


}


//*****************************************************************************
// MODULE	:	thread_relay_node
// ABSTRACT	:	中継ノード処理
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	NULL固定
// NOTE		:
//*****************************************************************************
void* thread_relay_node(void* p)
{
	thread_param_t*	param_p = p;
	void*			status;														// スレッド返却値
	int				ret;														// 汎用戻り値
	char			cmd[1024];													// コマンドラインバッファ
	
	// 引数を保持
	DBG(param_p,"[%d]: %s start\n", param_p->thread_no, __func__ );
	DBG(param_p,"[%d]: param_p->ip         = %s\n", param_p->thread_no, param_p->ip );
	DBG(param_p,"[%d]: param_p->port       = %d\n", param_p->thread_no, param_p->port );
	DBG(param_p,"[%d]: param_p->size       = %ld\n", param_p->thread_no, param_p->size );
	DBG(param_p,"[%d]: param_p->buf_size   = %ld\n", param_p->thread_no, param_p->buf_size );


	LOG(param_p,"Relay node enter \n");

	// 結果出力スレッドを起動する（受信計測用）
	ret = pthread_create( &param_p->output_thread_handle_r, NULL, (void *)recv_thread_output, (void *)param_p );
	if (ret != 0) {
		ERR(param_p,"can not create thread: %s", strerror(errno) );
	}
	while( (param_p->is_run_calc_r == STS_CALC_STOP) && (param_p->error_flg == 0) ) usleep(1);

	// アプリケーションモードの場合
	if( param_p->app_mode == 1 ){

		// 受信処理を呼び出す
		ret = recv_func_with_datasave(param_p);
		if( ret != 0 ){
			//処理継続
			goto EXIT_RTN;
		}

		// 結果出力スレッドを起動する（送信計測用）
		ret = pthread_create( &param_p->output_thread_handle_s, NULL, (void *)send_thread_output, (void *)param_p );
		if (ret != 0) {
			ERR(param_p,"can not create thread: %s", strerror(errno) );
		}
		while( (param_p->is_run_calc_s == STS_CALC_STOP) && (param_p->error_flg == 0) ) usleep(1);

		// アプリケーションモードの場合、外部プログラムを呼出す
		if( param_p->app_mode == 1 ){
			DBG(param_p, "app call start<%s>\n", param_p->app_filename );
			sprintf( cmd, "./%s", param_p->app_filename );
			system( cmd );
			DBG(param_p, "app call end\n" );
		}

		// 全受信サイズを次の送信サイズに設定する
		param_p->size = (size_t)(param_p->recv_total);

		LOG(param_p,"Relay node send start\n");

		// 送信処理を呼び出す
		ret = send_func(param_p);

		LOG(param_p,"Relay node send end\n");

		if( ret != 0 ){
			//処理継続
			goto EXIT_RTN;
		}

	}else{

		// 結果出力スレッドを起動する（送信計測用）
		ret = pthread_create( &param_p->output_thread_handle_s, NULL, (void *)send_thread_output, (void *)param_p );
		if (ret != 0) {
			ERR(param_p,"can not create thread: %s", strerror(errno) );
		}
		while( (param_p->is_run_calc_s == STS_CALC_STOP) && param_p->error_flg == 0 ) usleep(1);

		// トランスポート処理を呼び出す
		ret = tran_func(param_p);

		if( ret != 0 ){
			//処理継続
			goto EXIT_RTN;
		}
	}

	// 結果出力スレッドの終了を待つ
	pthread_join( param_p->output_thread_handle_r, &status );
	pthread_join( param_p->output_thread_handle_s, &status );

EXIT_RTN:

	LOG(param_p,"Relay node exit\n");

	DBG(param_p,"[%d]: %s end\n", param_p->thread_no, __func__ );

	// スレッド情報の解放
	release_thread(param_p);
	pthread_detach(pthread_self());
	pthread_exit(NULL);
}


//*****************************************************************************
// MODULE	:	send_func
// ABSTRACT	:	送信処理
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	0 固定
// NOTE		:
//*****************************************************************************
int send_func(thread_param_t* param_p)
{
	struct sockaddr_in	server;													// サーバーソケット情報格納変数
	ssize_t 			w_size;													// 送信データサイズ一時格納用
	size_t				send_size;												// 送信サイズ
	long long			lest_size = 0;											// 残り送信サイズ
	FILE*				output_fp = NULL;										// 外部出力ファイルポインタ
	size_t				file_r_size;											// 外部出力ファイル読み出しサイズ
	ssize_t				s_interval_size = 0;;									// 周期単位の送信サイズ
	socklen_t			optlen;													// オプション設定値長
	struct timeval		tv = {};												// 送信タイマー値
	int					errno_bk;												// エラー番号(退避用)
	struct stat			f_stat;													// 外部出力ファイル属性格納変数
	struct timespec	send_func_req={0,2};										// タスクスイッチ用sleep変数(1ns)


	// バッファの確保
	param_p->buf = malloc( param_p->buf_size );
	if( param_p->buf == (char*)NULL ){
		ERR(param_p,"Out of memory\n" );
		goto QUIT_RTN;
	}
	memset( param_p->buf, 0, (size_t)param_p->buf_size );

	// Startログ出力
	if( param_p->mode == MODE_SEND ){
		LOG(param_p,"Start port %d\n", param_p->port );
		LOG(param_p,"Test: %s\n", param_p->result_filename );
	}

	// ソケットの準備
	param_p->sock_cli = socket(AF_INET, SOCK_STREAM, 0);
	if (param_p->sock_cli < 0) {
	 	ERR(param_p,"socket create error\n");
		goto QUIT_RTN;
	}

	// 送信先のIPアドレスとポート番号を設定
	server.sin_family = AF_INET;
	server.sin_port = htons(param_p->port);
	server.sin_addr.s_addr = inet_addr( param_p->ip );
	if (server.sin_addr.s_addr == 0xffffffff) {
	 	ERR(param_p,"inet_addr error ip=%s\n", param_p->ip);
		goto QUIT_RTN;
	}

	//ソケットのtcp_congestionの設定
#ifndef __APPLE__
	if( param_p->congestion_mode == 1 ){
		if(setsockopt(param_p->sock_cli, IPPROTO_TCP, TCP_CONGESTION, param_p->congestion_name, strlen(param_p->congestion_name)) == -1){
		 	ERR(param_p,"setsockopt error (TCP_CONGESTION)\n");
			goto QUIT_RTN;
		}
	}
#endif
	// コネクト
	if (connect(param_p->sock_cli,(struct sockaddr *)&server, sizeof(server)) != 0) {
		ERR(param_p,"connect error ip=%s, port=%d\n", param_p->ip, param_p->port  );
		goto QUIT_RTN;
	}

	// 送信開始時刻になるまで Wait
	start_wait( param_p->send_start_time );

	// ソケットを非ブロッキングモードに設定
	int flags = fcntl(param_p->sock_cli, F_GETFL, 0);
	fcntl(param_p->sock_cli, F_SETFL, flags | O_NONBLOCK);

	// 送信タイマーの設定
	tv.tv_sec=param_p->timeout;
    optlen=sizeof(tv);
	if(setsockopt(param_p->sock_cli,SOL_SOCKET,SO_SNDTIMEO,&tv,optlen)==-1){
		ERR(param_p,"SO_SNDTIMEO set error ip=%s, port=%d\n", param_p->ip, param_p->port  );
		goto QUIT_RTN;
	}

	// アプリケーションモードの場合(中継ノード用)
	if( param_p->app_mode == 1 ){
		// outputファイルをオープン
		output_fp = fopen(param_p->output_filename, "rb");
		if (output_fp == NULL) {
			ERR(param_p, "Can't open file %s\n", param_p->output_filename );
			goto QUIT_RTN;
		}
		// outputファイルサイズを取得する
	    if (stat(param_p->output_filename, &f_stat) == 0){
			lest_size = f_stat.st_size;
		}
	}else{
		lest_size = param_p->size;
	}
	// 送信サイズが 0 byteならエラーメッセージを表示して終了する
	if( lest_size == 0 ){
		ERR(param_p, "output file size zero <%s>\n", param_p->output_filename );
		goto QUIT_RTN;
	}

	int	check_sendbuff;										// 確認するソケットの受信バッファサイズ
	// 今の受信バッファサイズを確認
	if (getsockopt(param_p->sock_cli, SOL_SOCKET, SO_SNDBUF, (void *)&check_sendbuff, &optlen) == -1) {
		ERR(param_p, "SO_SNDBUF get error ip=%s, port=%d\n", param_p->ip, param_p->port);
		goto QUIT_RTN;
	}
	LOG(param_p, "buff_size %d\n", check_sendbuff);

	// 送信処理
	DBG(param_p,"send start\n" );
	while( (0 < lest_size) && param_p->error_flg == 0 ){

		// 1送信のパケットサイズを算出
		if( param_p->buf_size <= lest_size ){
			send_size = param_p->buf_size;
		}else{
			send_size = lest_size;
		}

		// アプリケーションモードの場合(中継ノード用)
		if( param_p->app_mode == 1 ){
			if (output_fp != NULL) {
				file_r_size = fread(param_p->buf, 1, send_size, output_fp);
				if( file_r_size < send_size ){
					send_size = file_r_size;
					if( send_size <= 0 ){
						lest_size =0;
						continue;
					}
				}
			}
		}

		// 2023.01.23 ADD START 結果出力スレッド(送信用)に計測開始を知らせる
		if( param_p->is_run_calc_s != STS_CALC_MEASURING ){
			param_p->is_run_calc_s = STS_CALC_START;
		}
		// 2023.01.23 ADD END   結果出力スレッド(送信用)に計測開始を知らせる

		// 送信処理
		w_size = send(param_p->sock_cli, param_p->buf, send_size, 0 );
		
		errno_bk = errno;
		if ( param_p->dbg_mode2 == 1 ){
			DBG(param_p,"send data(send_size=%ld, w_size=%ld, errno=%d)\n",  send_size, w_size, errno_bk );
		}
		if (w_size < 0) {
			if( errno_bk==EWOULDBLOCK || errno_bk==EAGAIN ){
				if( wait_for_socket(param_p, param_p->sock_cli, 10000, 1, 0) == 1){
					continue;
				}else{
					ERR(param_p,"send wait_for_socket error\n");
					break;
				}
			}else{
				ERR(param_p,"send error\n");
				break;
			}
		}
		lest_size -= w_size;
		s_interval_size += w_size;
		param_p->send_total += w_size;

		nanosleep(&send_func_req, NULL);
	}

	// 今の受信バッファサイズを確認
	if (getsockopt(param_p->sock_cli, SOL_SOCKET, SO_SNDBUF, (void *)&check_sendbuff, &optlen) == -1) {
		ERR(param_p, "SO_SNDBUF get error ip=%s, port=%d\n", param_p->ip, param_p->port);
		goto QUIT_RTN;
	}
	LOG(param_p, "buff_size %d\n", check_sendbuff);

QUIT_RTN:
	// 2023.01.23 ADD START 結果出力スレッド(送信用)が計測を開始しているか確認する
	if( 0 < param_p->send_total ){
		// 1byteでも送信されていれば、結果出力スレッドが開始されているか確認し
		// 開始されていなければ、開始されるのを待ってから終了処理を行うようにする
		while( param_p->is_run_calc_s != STS_CALC_MEASURING ){
			usleep(0);
		}
	}
	// 2023.01.23 ADD END   結果出力スレッド(送信用)が計測を開始しているか確認する

	// 結果出力スレッド(送信用)に終了指示を行う
	calc_stop(param_p, 0);

	DBG(param_p,"send end send_total = %ld\n", param_p->send_total );

	// ソケットをShutdownする
	shutdown( param_p->sock_cli, SHUT_WR );

	// 結果出力スレッド起動中フラグをOFFにする(送信計測用)
	if( param_p->is_run_calc_s == STS_CALC_WAIT ){
		param_p->is_run_calc_s = STS_CALC_STOP;
	}

	// ソケットをクローズする
	if( 0 < param_p->sock_cli ){
		close(param_p->sock_cli);
		param_p->sock_cli = 0;
	}

	// バッファを解放する
	if( param_p->buf != NULL ){
		free(param_p->buf); param_p->buf = NULL;
	}

	// 出力結果ファイルがオープン中であればクローズする
	if( output_fp != NULL ){
		fclose(output_fp);
		output_fp = NULL;
	}

	return 0;
}


//*****************************************************************************
// MODULE	:	recv_func
// ABSTRACT	:	受信処理
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	0固定
// NOTE		:
//*****************************************************************************
int recv_func(thread_param_t* param_p)
{
	struct sockaddr_in	addr;													// 受信待ちソケット情報格納変数
	struct sockaddr_in	client;													// クライアントソケット情報格納変数
	socklen_t			len;													// クライアントソケット情報長
	ssize_t				r_size;													// 受信データサイズ(一時格納用)
	size_t				recv_size;												// 受信データサイズ
	fd_set				readfds;												// 受信ファイルディスクプリタ(読み込み用)
	fd_set				fds;													// 受信ファイルディスクプリタ(汎用)
	int					ret;													// 関数戻り値
	bool				yes = 1;												// オプション設定値
	ssize_t				r_interval_size = 0;;									// 周期単位の受信サイズ
	socklen_t			optlen;													// オプション設定値長
	struct timeval		tv = {};												// 受信タイマー値
	int					errno_bk;												// エラー番号(退避用)
	// struct	timespec	recv_func_req={0,2};

	// バッファの確保
	param_p->buf = malloc( param_p->buf_size );
	if( param_p->buf == (char*)NULL ){
		ERR(param_p, "Out of memory\n" );
		goto QUIT_RTN;
	}
	memset( param_p->buf, 0, (size_t)param_p->buf_size );

	// ソケットの準備
	param_p->sock_acc = socket(AF_INET, SOCK_STREAM, 0);
	if (param_p->sock_acc < 0) {
	 	ERR(param_p,"socket create error\n");
		goto QUIT_RTN;
	}

	// ソケットオプションの設定
 	setsockopt(param_p->sock_acc, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	// バインド
	while( param_p->error_flg == 0 ){
		addr.sin_family = AF_INET;
		addr.sin_port = htons(param_p->port);
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(param_p->sock_acc, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
			if( param_p->auto_port == 1 ){
				param_p->port ++;
				continue;
			}
			ERR(param_p,"bind error port=%d\n", param_p->port);
			goto QUIT_RTN;
		}
		break;
	}

	// リッスン
	if (listen(param_p->sock_acc, 5) != 0) {
		ERR(param_p,"listen error port=%d\n", param_p->port);
		goto QUIT_RTN;
	}

	// Startログ出力
	LOG(param_p, "Start port %d\n", param_p->port );
	LOG(param_p,"Test: %s\n", param_p->result_filename );

	// 接続待ち(アクセプト)
	DBG(param_p,"accept wait... port=%d\n", param_p->port);
	len = sizeof(client);
	param_p->sock_svr = accept(param_p->sock_acc, (struct sockaddr *)&client, &len);
	if (param_p->sock_svr < 0) {
		ERR(param_p,"accept error\n");
		goto QUIT_RTN;
	}
	DBG(param_p,"accept OK\n");
	FD_ZERO( &readfds );
	FD_SET( param_p->sock_svr, &readfds );

	// 受信待ち（recv）を行う
	//   受信開始タイミングを得るため、recv待ちではなくselectで待つ
	while( param_p->error_flg == 0 ){
		DBG(param_p,"recv wait... (socket=%d)\n", param_p->sock_svr );
		memcpy(&fds, &readfds, sizeof(fd_set));
		ret = select((int)(param_p->sock_svr + 1), &fds, NULL, NULL, NULL);
		if (ret <= 0) {
			if( ret == 0 ){
				continue;
			}
			ERR(param_p, "select error\n");
			goto QUIT_RTN;
		}
		break;
	}

	// ソケットを非ブロッキングモードに設定
	int flags = fcntl(param_p->sock_svr, F_GETFL, 0);
	fcntl(param_p->sock_svr, F_SETFL, flags | O_NONBLOCK);

	// 受信タイマーの設定
	tv.tv_sec=param_p->timeout;
    optlen=sizeof(tv);
	if(setsockopt(param_p->sock_svr,SOL_SOCKET,SO_RCVTIMEO,&tv,optlen)==-1){
		ERR(param_p,"SO_RCVTIMEO set error ip=%s, port=%d\n", param_p->ip, param_p->port  );
		goto QUIT_RTN;
	}

	DBG(param_p,"recv start (select value=%d)\n", ret);

	int	check_recvbuff;										// 確認するソケットの受信バッファサイズ
	// 通信開始前の受信バッファサイズを確認
	if (getsockopt(param_p->sock_svr, SOL_SOCKET, SO_RCVBUF, (void *)&check_recvbuff, &optlen) == -1) {
		ERR(param_p, "SO_RCVBUF get error ip=%s, port=%d\n", param_p->ip, param_p->port);
		goto QUIT_RTN;
	}
	LOG(param_p, "buff_size %d\n", check_recvbuff);

	// 受信処理
	recv_size = param_p->buf_size;
	while( param_p->error_flg == 0 ){

		// 2023.01.23 ADD START 結果出力スレッド(受信用)に計測開始を知らせる
		if( param_p->is_run_calc_r != STS_CALC_MEASURING ){
			param_p->is_run_calc_r = STS_CALC_START;
		}
		// 2023.01.23 ADD END   結果出力スレッド(受信用)に計測開始を知らせる

		r_size = recv(param_p->sock_svr, &param_p->buf[0], recv_size, 0);
		
		errno_bk=errno;
		if ( param_p->dbg_mode2 == 1 ){
			DBG(param_p,"recv data(recv_size=%ld, r_size=%ld)\n",  recv_size, r_size );
		}
		if( r_size < 0 ){
			if( errno_bk==EWOULDBLOCK || errno_bk==EAGAIN ){
				if( wait_for_socket(param_p, param_p->sock_svr, 10000, 0, 1) == 1){
					continue;
				}else{
					ERR(param_p,"recv error\n");
				}
			}	
		}else if( r_size == 0){
			DBG(param_p,"recv stop\n");

			// 通信終了時の受信バッファサイズを確認
			if (getsockopt(param_p->sock_svr, SOL_SOCKET, SO_RCVBUF, (void *)&check_recvbuff, &optlen) == -1) {
				ERR(param_p, "SO_RCVBUF get error ip=%s, port=%d\n", param_p->ip, param_p->port);
				goto QUIT_RTN;
			}
			LOG(param_p, "buff_size %d\n", check_recvbuff);

			close(param_p->sock_svr);
			param_p->sock_svr = 0;
			break;
		}

		r_interval_size += r_size;
		param_p->recv_total += r_size;
	}

QUIT_RTN:

	// 2023.01.23 ADD START 結果出力スレッド(受信用)が計測を開始しているか確認する
	if( 0 < param_p->recv_total ){
		// 1byteでも送信されていれば、結果出力スレッドが開始されているか確認し
		// 開始されていなければ、開始されるのを待ってから終了処理を行うようにする
		while( param_p->is_run_calc_r != STS_CALC_MEASURING ){
			usleep(0);
		}
	}
	// 2023.01.23 ADD END   結果出力スレッド(受信用)が計測を開始しているか確認する

	// 結果出力スレッド(受信用)に終了指示を行う
	calc_stop(param_p, 1);

	DBG(param_p,"recv end recv_total=%ld\n", param_p->recv_total );

	// 結果出力スレッド起動中フラグをOFFにする(受信計測用)
	if( param_p->is_run_calc_r == STS_CALC_WAIT ){
		param_p->is_run_calc_r = STS_CALC_STOP;
	}

	// バッファを解放する
	if( param_p->buf != NULL ){
		free(param_p->buf);
		param_p->buf = NULL;
	}

	// ソケットをクローズする(アクセプト用)
	if( 0 < param_p->sock_acc ){
		close(param_p->sock_acc);
		param_p->sock_acc = 0;
	}

	// ソケットをクローズする(受信用)
	if( 0 < param_p->sock_svr ){
		close(param_p->sock_svr);
		param_p->sock_svr = 0;
	}
	return 0;
}


//*****************************************************************************
// MODULE	:	is_bufover
// ABSTRACT	:	バッファオーバー判定処理
// FUNCTION	:
// ARGUMENTS:	int	recv_total	受信したトータルデータサイズ
//			:	int	send_total	送信したトータルデータサイズ
//			:	int	data_size	データサイズ
//			:	int	buf_size	バッファサイズ
// RETURN	:	0 = 書き込み可能
//			:	1 = 書き込み不可(バッファーオーバーする)
// NOTE		:
//*****************************************************************************
bool is_bufover( int recv_total, int send_total, int data_size, int buf_size )
{
	int next_recv = recv_total + data_size;		// 次のrecv_total
	int check = 0;								// 戻り値
	
	if ( next_recv - send_total >= buf_size ){
		check = 1;
	}
	return check;
}


//*****************************************************************************
// MODULE	:	recv_func_with_datasave
// ABSTRACT	:	受信スレッド(データ保存付き)
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	0固定
// NOTE		:
//*****************************************************************************
int recv_func_with_datasave(thread_param_t* param_p)
{
	struct sockaddr_in	addr;													// 受信待ちソケット情報格納変数
	struct sockaddr_in	client;													// クライアントソケット情報格納変数
	socklen_t			len;													// クライアントソケット情報長
	ssize_t				r_size;													// 受信データサイズ(一時格納用)
	ssize_t				recv_size;												// 受信データサイズ
	fd_set				readfds;												// 受信ファイルディスクプリタ(読み込み用)
	fd_set				fds;													// 受信ファイルディスクプリタ(汎用)
	int					ret;													// 関数戻り値
	bool				yes = 1;												// オプション設定値
	void*				status;													// スレッド返却値
	ssize_t				r_interval_size = 0;;									// 周期単位の受信サイズ
	char*				buf_p;													// バッファのポインタ(一時格納用)
	int					w_pos;													// バッファ書き込みポインタ
	int					disp_flg = 0;											// バッファオーバーエラー出力フラグ
	int					check;													// バッファオーバー判定用変数
	struct	timespec	req ={ 0, 1 };											// タスクスイッチ用sleep変数(1ns)
	struct	timeval		tv = {};												// 受信タイマー値
	pthread_t			save_thread_handle;										// データ保存スレッドハンドル
	bool				is_save_thread_run = 0;									// データ保存スレッド実行中フラグ
	socklen_t			optlen;													// オプション設定値長
	int					recvbuff;												// カーネルの受信バッファサイズ
	int					errno_bk;												// エラー番号(退避用)

	// バッファの確保
	param_p->buf = malloc( param_p->ring_bufsize );
	if( param_p->buf == (char*)NULL ){
		ERR(param_p, "Out of memory\n" );
		goto QUIT_RTN;
	}
	memset( param_p->buf, 0, (size_t)param_p->buf_size );

	// [受信側]ソケットの準備
	param_p->sock_acc = socket(AF_INET, SOCK_STREAM, 0);
	if (param_p->sock_acc < 0) {
	 	ERR(param_p,"socket create error\n");
		goto QUIT_RTN;
	}

	// [受信側]ソケットオプションの設定
 	setsockopt(param_p->sock_acc, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	// [受信側]バインド
	while( param_p->error_flg == 0 ){
		addr.sin_family = AF_INET;
		addr.sin_port = htons(param_p->port);
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(param_p->sock_acc, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
			if( param_p->auto_port == 1 ){
				param_p->port ++;
				continue;
			}
			ERR(param_p,"bind error port=%d\n", param_p->port);
			goto QUIT_RTN;
		}
		break;
	}

	// [受信側]リッスン
	if (listen(param_p->sock_acc, 5) != 0) {
		ERR(param_p,"listen error port=%d\n", param_p->port);
		goto QUIT_RTN;
	}

	// Startログ出力
	LOG(param_p, "Start port %d\n", param_p->port );
	LOG(param_p,"Test: %s\n", param_p->result_filename );

	// [受信側]接続待ち(アクセプト)
	DBG(param_p,"accept wait port=%d\n", param_p->port);
	len = sizeof(client);
	param_p->sock_svr = accept(param_p->sock_acc, (struct sockaddr *)&client, &len);
	if (param_p->sock_svr < 0) {
		ERR(param_p,"accept error\n");
		goto QUIT_RTN;
	}
	DBG(param_p,"accept OK\n");
	FD_ZERO( &readfds );
	FD_SET( param_p->sock_svr, &readfds );

	// カーネルの受信バッファの設定
	optlen = sizeof(recvbuff);
	recvbuff = param_p->rcv_bufsize;
	if( setsockopt(param_p->sock_svr, SOL_SOCKET, SO_RCVBUF, (void *)&recvbuff, optlen)==-1 ){
		ERR(param_p,"SO_RCVBUF set error ip=%s, port=%d\n", param_p->ip, param_p->port  );
		goto QUIT_RTN;
	}

	// 受信タイマーの設定
	tv.tv_sec=param_p->timeout;
    optlen=sizeof(tv);
	if(setsockopt(param_p->sock_svr,SOL_SOCKET,SO_RCVTIMEO,&tv,optlen)==-1){
		ERR(param_p,"SO_RCVTIMEO set error ip=%s, port=%d\n", param_p->ip, param_p->port  );
		goto QUIT_RTN;
	}

	// データ保存スレッド起動
	ret = pthread_create( &save_thread_handle, NULL, (void *)thread_save, (void *)param_p );
	if (ret != 0) {
		ERR(param_p,"create send thread error\n");
		goto QUIT_RTN;
	}
	while( (param_p->is_run_save == 0) && (param_p->error_flg == 0) ) usleep(1);
	is_save_thread_run = 1;

	// 受信待ち（recv）を行う
	//   受信開始タイミングを得るため、recv待ちではなくselectで待つ
	while( param_p->error_flg == 0 ){

		DBG(param_p,"recv wait... (socket=%d)\n", param_p->sock_svr );
		memcpy(&fds, &readfds, sizeof(fd_set));
		ret = select((int)(param_p->sock_svr + 1), &fds, NULL, NULL, NULL);
		if (ret <= 0) {
			DBG(param_p, "select ret=>%d, <%s>\n", ret, strerror(errno) );
			if( ret == 0 ){
				continue;
			}
			ERR(param_p, "select error\n");
			goto QUIT_RTN;
		}
		break;
	}

	DBG(param_p,"recv start (select value=%d)\n", ret);
	r_size = 0;
	recv_size = param_p->buf_size;

	// 受信処理
	while( param_p->error_flg == 0 ){

		// 受信用リングバッファの受信データ格納位置を算出する
		w_pos = param_p->save_w_pos;
		buf_p = param_p->buf + w_pos;
		if( param_p->ring_bufsize < (w_pos + param_p->buf_size) ) {
			recv_size = param_p->ring_bufsize - w_pos;
		}else{
			recv_size = param_p->buf_size;
		}

		// 2023.01.23 ADD START 結果出力スレッド(受信用)に計測開始を知らせる
		if( param_p->is_run_calc_r != STS_CALC_MEASURING ){
			param_p->is_run_calc_r = STS_CALC_START;
		}
		// 2023.01.23 ADD END   結果出力スレッド(受信用)に計測開始を知らせる

		// 受信
		r_size = recv(param_p->sock_svr, buf_p, recv_size, 0);
		errno_bk=errno;
		if ( param_p->dbg_mode2 == 1 ){
			DBG(param_p,"recv data(recv_size=%ld, r_size=%ld)\n",  recv_size, r_size );
		}
		if (r_size < 1) {
			if( r_size != 0 ){
				if( errno_bk==EWOULDBLOCK || errno_bk==EAGAIN ){
					ERR(param_p,"recv timeout\n");
				}else{
					ERR(param_p,"recv error\n");
				}
			}
			DBG(param_p,"recv stop\n");
			break;
		}

		r_interval_size += r_size;
		param_p->recv_total += r_size;

		// バッファの空き領域を確認
		check = 1;
		disp_flg = 0;
		while( (check == 1) && (param_p->error_flg == 0) ){
			// check = is_bufover( w_pos,  param_p->save_r_pos, r_size, param_p->ring_bufsize );
			if( check == 1 ){
				if( disp_flg == 0 ){
					LOG(param_p, "recv buffer full... wait until free\n" );
					disp_flg = 1;
				}
				nanosleep(&req, NULL);
				continue;
			}
			check=0;
			break;
		}

		// 受信用リングバッファのポインタを進める
		if( param_p->ring_bufsize < (w_pos + r_size) ) {
			param_p->save_w_pos = (w_pos + r_size) - param_p->ring_bufsize;
		}else{
			w_pos += r_size;
			if( param_p->ring_bufsize <= w_pos ){
				param_p->save_w_pos = 0;
			}else{
				param_p->save_w_pos = w_pos;
			}
		}
	}

QUIT_RTN:

	// 2023.01.23 ADD START 結果出力スレッド(受信用)が計測を開始しているか確認する
	if( 0 < param_p->recv_total ){
		// 1byteでも送信されていれば、結果出力スレッドが開始されているか確認し
		// 開始されていなければ、開始されるのを待ってから終了処理を行うようにする
		while( param_p->is_run_calc_r != STS_CALC_MEASURING ){
			usleep(0);
		}
	}
	// 2023.01.23 ADD END   結果出力スレッド(受信用)が計測を開始しているか確認する

	// 結果出力スレッド(受信用)に終了指示を行う
	calc_stop(param_p, 1);

	DBG(param_p, "[%d, %d] recv last\n", param_p->save_r_pos, param_p->save_w_pos );

	// データ保存スレッドの終了待ちを行う
	param_p->is_run_save = 0;
	if( is_save_thread_run == 1 ){
		pthread_join( save_thread_handle, &status );
	}
	DBG(param_p,"recv end recv_total=%ld, save_total=%ld\n", param_p->recv_total, param_p->save_total);

	// 結果出力スレッド起動中フラグをOFFにする(送信計測用/受信計測用)
	if( param_p->is_run_calc_s == STS_CALC_WAIT ){
		param_p->is_run_calc_s = STS_CALC_STOP;
	}
	if( param_p->is_run_calc_r == STS_CALC_WAIT ){
		param_p->is_run_calc_r = STS_CALC_STOP;
	}

	// ソケットをクローズする(アクセプト用)
	if( 0 < param_p->sock_acc ){
		close(param_p->sock_acc);
		param_p->sock_acc = 0;
	}

	// ソケットをクローズする(受信用)
	if( 0 < param_p->sock_svr ){
		close(param_p->sock_svr);
		param_p->sock_svr = 0;
	}

	// バッファの解放
	if( param_p->buf != NULL ){
		free(param_p->buf);
		param_p->buf = NULL;
	}
	return 0;
}


//*****************************************************************************
// MODULE	:	tran_func
// ABSTRACT	:	トランスポート処理
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	0固定
// NOTE		:
//*****************************************************************************
int tran_func(thread_param_t* param_p)
{
	struct sockaddr_in	addr;												// 受信待ちソケット情報格納変数
	struct sockaddr_in	client;												// クライアントソケット情報格納変数
	struct sockaddr_in	server;												// サーバーソケット情報格納変数
	socklen_t			len;												// クライアントソケット情報長
	ssize_t				r_size;												// 受信データサイズ(一時格納用)
	ssize_t				recv_size;											// 受信データサイズ
	fd_set				readfds;											// 受信ファイルディスクプリタ(読み込み用)
	fd_set				fds;												// 受信ファイルディスクプリタ(汎用)
	int					ret;												// 関数戻り値
	bool				yes = 1;											// オプション設定値
	void*				status;												// スレッド返却値
	ssize_t				r_interval_size = 0;;								// 周期単位の受信サイズ
	char*				buf_p;												// バッファのポインタ(一時格納用)
	int					w_pos;												// バッファ書き込みポインタ
	int					disp_flg = 0;										// バッファオーバーエラー出力フラグ
	int					check;												// バッファオーバー判定用変数
	struct	timeval		tv = {};											// 受信タイマー値
	pthread_t			send_thread_handle;									// 送信スレッドハンドル
	bool				is_send_thread_run = 0;								// 送信スレッド実行中フラグ
	socklen_t			optlen;												// オプション設定値長
	int					errno_bk;											// エラー番号(退避用)

	int					target=128000;										// target [byte]
	struct	timespec	tran_recv_req={0,2000000};							// control interval [nano sec]
	ssize_t				gap_datasize;										// targetと蓄積してるデータサイズの差(一時格納用)
	int					check_recvbuff;										// 確認するソケットの受信バッファサイズ
	int					loop_counter = 0;

	// バッファの確保
	param_p->buf = malloc( param_p->ring_bufsize );
	if( param_p->buf == (char*)NULL ){
		ERR(param_p, "Out of memory\n" );
		goto QUIT_RTN;
	}
	memset( param_p->buf, 0, (size_t)param_p->buf_size );

	// [受信側]ソケットの準備
	param_p->sock_acc = socket(AF_INET, SOCK_STREAM, 0);
	if (param_p->sock_acc < 0) {
	 	ERR(param_p,"socket create error\n");
		goto QUIT_RTN;
	}

	// [送信側]ソケットの準備
	param_p->sock_cli = socket(AF_INET, SOCK_STREAM, 0);
	if (param_p->sock_cli < 0) {
	 	ERR(param_p,"socket create error\n");
		goto QUIT_RTN;
	}

	// [送信側]ソケットのtcp_congestionの設定
#ifndef __APPLE__
	if( param_p->congestion_mode == 1 ){
		if(setsockopt(param_p->sock_cli, IPPROTO_TCP, TCP_CONGESTION, param_p->congestion_name, strlen(param_p->congestion_name)) == -1){
		 	ERR(param_p,"setsockopt error (TCP_CONGESTION)\n");
			goto QUIT_RTN;
		}
	}
#endif

	// [送信側]送信先(server)のIPアドレスとポート番号を設定
	server.sin_family = AF_INET;
	server.sin_port = htons(param_p->port);
	server.sin_addr.s_addr = inet_addr( param_p->ip );
	if (server.sin_addr.s_addr == 0xffffffff) {
	 	ERR(param_p,"inet_addr error ip=%s\n", param_p->ip);
		goto QUIT_RTN;
	}
	
	// [送信側]コネクト
	if (connect(param_p->sock_cli,(struct sockaddr *)&server, sizeof(server)) != 0) {
		ERR(param_p,"connect error ip=%s, port=%d\n", param_p->ip, param_p->port  );
		goto QUIT_RTN;
	}
	


	// [受信側]ソケットオプションの設定
 	setsockopt(param_p->sock_acc, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	// [受信側]バインド
	while( param_p->error_flg == 0 ){

		addr.sin_family = AF_INET;
		addr.sin_port = htons(param_p->port);
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(param_p->sock_acc, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
			if( param_p->auto_port == 1 ){
				param_p->port ++;
				continue;
			}
			ERR(param_p,"bind error port=%d\n", param_p->port);
			goto QUIT_RTN;
		}
		break;
	}

	// [受信側]リッスン
	if (listen(param_p->sock_acc, 5) != 0) {
		ERR(param_p,"listen error port=%d\n", param_p->port);
		goto QUIT_RTN;
	}

	// Startログ出力
	LOG(param_p, "Start port %d\n", param_p->port );
	LOG(param_p,"Test: %s\n", param_p->result_filename );


	// [受信側]接続待ち(アクセプト)
	DBG(param_p,"accept wait port=%d\n", param_p->port);
	len = sizeof(client);
	param_p->sock_svr = accept(param_p->sock_acc, (struct sockaddr *)&client, &len);
	if (param_p->sock_svr < 0) {
		ERR(param_p,"accept error\n");
		goto QUIT_RTN;
	}
	DBG(param_p,"accept OK\n");
	FD_ZERO( &readfds );
	FD_SET( param_p->sock_svr, &readfds );

	// 受信側のソケットを非ブロッキングモードに設定
	int recv_sock_flags = fcntl(param_p->sock_svr, F_GETFL, 0);
	fcntl(param_p->sock_svr, F_SETFL, recv_sock_flags | O_NONBLOCK);

	// 受信タイマーの設定
	tv.tv_sec=param_p->timeout;
    optlen=sizeof(tv);
	if(setsockopt(param_p->sock_svr,SOL_SOCKET,SO_RCVTIMEO,&tv,optlen)==-1){
		ERR(param_p,"SO_RCVTIMEO set error ip=%s, port=%d\n", param_p->ip, param_p->port  );
		goto QUIT_RTN;
	}

	// 送信側のソケットを非ブロッキングモードに設定
	int send_sock_flags = fcntl(param_p->sock_cli, F_GETFL, 0);
	fcntl(param_p->sock_svr, F_SETFL, send_sock_flags | O_NONBLOCK);

	// 送信タイマーの設定
	tv.tv_sec=param_p->timeout;
    optlen=sizeof(tv);
	if(setsockopt(param_p->sock_cli,SOL_SOCKET,SO_SNDTIMEO,&tv,optlen)==-1){
		ERR(param_p,"SO_SNDTIMEO set error ip=%s, port=%d\n", param_p->ip, param_p->port  );
		goto QUIT_RTN;
	}

	// 送信スレッド起動
	ret = pthread_create( &send_thread_handle, NULL, (void *)thread_send, (void *)param_p );
	if (ret != 0) {
		ERR(param_p,"create send thread error\n");
		goto QUIT_RTN;
	}
	while( (param_p->is_run_trans == 0) && (param_p->error_flg == 0) ) usleep(1);
	is_send_thread_run = 1;

	// 受信待ち（recv）を行う
	//   受信開始タイミングを得るため、recv待ちではなくselectで待つ
	while( param_p->error_flg == 0 ){
		DBG(param_p,"recv wait... (socket=%d)\n", param_p->sock_svr );
		memcpy(&fds, &readfds, sizeof(fd_set));
		ret = select((int)(param_p->sock_svr + 1), &fds, NULL, NULL, NULL);
		if (ret <= 0) {
			DBG(param_p, "select ret=>%d, <%s>\n", ret, strerror(errno) );
			if( ret == 0 ){
				continue;
			}
			ERR(param_p, "select error\n");
			goto QUIT_RTN;
		}
		break;
	}
	DBG(param_p,"recv start (select value=%d)\n", ret);

	// 通信開始前の受信バッファサイズを確認
	if (getsockopt(param_p->sock_svr, SOL_SOCKET, SO_RCVBUF, (void *)&check_recvbuff, &optlen) == -1) {
		ERR(param_p, "SO_RCVBUF get error ip=%s, port=%d\n", param_p->ip, param_p->port);
		goto QUIT_RTN;
	}
	LOG(param_p, "buff_size %d\n", check_recvbuff);

	LOG(param_p, "target [byte] %d\n", target);

	r_size = 0;
	recv_size = param_p->buf_size;

	// 受信処理
	while( param_p->error_flg == 0 ){
		// 毎回スリープ
		nanosleep(&tran_recv_req, NULL);

		// 受信用リングバッファの受信データ格納位置を算出する
		w_pos = param_p->trans_w_pos;
		buf_p = param_p->buf + w_pos;
		gap_datasize = target - (param_p->recv_total - param_p->send_total);

		// recv()に渡すデータサイズの計算
		if( gap_datasize > 0 ){
			if( gap_datasize >= param_p->buf_size ){
				recv_size = param_p->buf_size;
			}else{
				recv_size = gap_datasize;
			}

			// recv_sizeを受信するとリングバッファのポインタが確保した分を超える時
			if( param_p->ring_bufsize < (w_pos + recv_size)){
				recv_size = param_p->ring_bufsize - w_pos;
			}
		}else{
			continue;
		}

		// 2023.01.23 ADD START 結果出力スレッド(受信用)に計測開始を知らせる
		if( param_p->is_run_calc_r != STS_CALC_MEASURING ){
			param_p->is_run_calc_r = STS_CALC_START;
		}
		// 2023.01.23 ADD END   結果出力スレッド(受信用)に計測開始を知らせる

		r_size = recv(param_p->sock_svr, buf_p, recv_size, 0);

		errno_bk=errno;
		if ( param_p->dbg_mode2 == 1 ){
			DBG(param_p,"recv data(recv_size=%ld, r_size=%ld)\n",  recv_size, r_size );
		}

		if (r_size < 1){
			if(r_size != 0){
				if(errno_bk == EWOULDBLOCK || errno_bk==EAGAIN){
					if(wait_for_socket(param_p, param_p->sock_svr, 10000, 0, 1) == 1){
						continue;
					}else{
						ERR(param_p,"recv error\n");
						break;
					}
				}else{
					ERR(param_p,"recv error\n");
					break;
				}
			}else{
				DBG(param_p,"recv stop\n");
				break;
			}
		} 
		r_interval_size += r_size;
		param_p->recv_total += r_size;

		// バッファの空き領域を確認
		check = 1;
		disp_flg = 0;
		while( (check == 1) && (param_p->error_flg == 0) ){
			check = is_bufover( param_p->recv_total,  param_p->send_total, r_size, param_p->ring_bufsize );
			if( check == 1 ){
				if( disp_flg == 0 ){
					LOG(param_p, "recv buffer full... wait until free\n" );
					disp_flg = 1;
				}
				
				continue;
			}
			check=0;
			break;
		}

		// 受信用リングバッファのポインタを進める
		if( param_p->ring_bufsize < (w_pos + r_size) ) {
			param_p->trans_w_pos = (w_pos + r_size) - param_p->ring_bufsize;
		}else{
			w_pos += r_size;
			if( param_p->ring_bufsize <= w_pos ){
				param_p->trans_w_pos = 0;
			}else{
				param_p->trans_w_pos = w_pos;
			}
		}

		loop_counter += 1;
		if (loop_counter > 10){
			LOG(param_p, "recv_size %ld send_size %ld\n", param_p->recv_total, param_p->send_total);
			loop_counter = 0;
		}
	}

	// 受信終了後の受信バッファサイズ
	getsockopt(param_p->sock_svr, SOL_SOCKET, SO_RCVBUF, (void *)&check_recvbuff, &optlen);
	LOG(param_p, "buff_size %d\n", check_recvbuff);

QUIT_RTN:

	// 2023.01.23 ADD START 結果出力スレッド(受信用)が計測を開始しているか確認する
	if( 0 < param_p->recv_total ){
		// 1byteでも送信されていれば、結果出力スレッドが開始されているか確認し
		// 開始されていなければ、開始されるのを待ってから終了処理を行うようにする
		while( param_p->is_run_calc_r != STS_CALC_MEASURING ){
			usleep(0);
		}
	}
	// 2023.01.23 ADD END   結果出力スレッド(受信用)が計測を開始しているか確認する

	// 結果出力スレッド(受信用)に終了指示を行う
	calc_stop(param_p, 1);
	param_p->is_run_trans = 0;

	// 送信スレッドの終了待ちを行う
	if( is_send_thread_run == 1 ){
		pthread_join( send_thread_handle, &status );
	}

	// 結果出力スレッド起動中フラグをOFFにする(送信計測用/受信計測用)
	DBG(param_p,"trans end recv_total=%ld, send_total=%ld\n", param_p->recv_total, param_p->send_total);
	if( param_p->is_run_calc_s == STS_CALC_WAIT ){
		param_p->is_run_calc_s = STS_CALC_STOP;
	}
	if( param_p->is_run_calc_r == STS_CALC_WAIT ){
		param_p->is_run_calc_r = STS_CALC_STOP;
	}

	// ソケットをクローズする(送信用)
	if( 0 < param_p->sock_cli ){
		close(param_p->sock_cli);
		param_p->sock_cli = 0;
	}

	// ソケットをクローズする(アクセプト用)
	if( 0 < param_p->sock_acc ){
		close(param_p->sock_acc);
		param_p->sock_acc = 0;
	}

	// ソケットをクローズする(受信用)
	if( 0 < param_p->sock_svr ){
		close(param_p->sock_svr);
		param_p->sock_svr = 0;
	}

	// バッファの解放
	if( param_p->buf != NULL ){
		free(param_p->buf);
		param_p->buf = NULL;
	}
	return 0;
}


//*****************************************************************************
// MODULE	:	thread_send
// ABSTRACT	:	トランスポートレベル専用送信処理
// FUNCTION	:
// ARGUMENTS:	void*	p		(In) 引数のポインタ
// RETURN	:	0固定
// NOTE		:
//*****************************************************************************
void* thread_send(void* p)
{
	thread_param_t*	param_p = (thread_param_t*)p;								// パラメータ格納変数
	ssize_t				w_size;													// 送信データサイズ(一時格納用)
	ssize_t				s_interval_size = 0;;									// 周期単位の送信サイズ
	ssize_t				r_size;													// 受信データサイズ(一時格納用)
	char*				buf_p;													// バッファのポインタ(一時格納用)
	int					w_pos;													// バッファ書き込みポインタ
	int					r_pos;													// バッファ読み込みポインタ
	int					top_move;												// リングバッファ先頭移動フラグ
	struct	timespec	tran_send_req={0,2};									// タスクスイッチ用sleep変数
	int					errno_bk;												// エラー番号(退避用)
	int					err_flg=0;												// エラー発生中フラグ

	DBG(param_p, "thread send start\n" );
	param_p->is_run_trans = 1;

	LOG(param_p,"Relay transport send start\n");

	// 送信処理
	while( param_p->error_flg == 0 ){
		// エラーが発生したら処理を終了する
		if( err_flg==1 ) break;

		// 別スレッドから終了要求を受けた場合、送信処理を終了する
		if( param_p->is_run_trans == 0 ){
			if( param_p->recv_total == param_p->send_total ){
				break;
			}
		}

		// リングバッファに送信するデータが存在するかチェックする
		while( (param_p->trans_r_pos != param_p->trans_w_pos) && (param_p->error_flg == 0) ){
			// 毎回スリープ
			nanosleep(&tran_send_req, NULL);

			// リングバッファの読み込みポインタ/リングバッファの書き込みポインタを取得
			w_pos = param_p->trans_w_pos;
			r_pos = param_p->trans_r_pos;

			// 送信サイズ算出
			if( w_pos >= r_pos){
				top_move = 0;
				if( param_p->buf_size <= w_pos - r_pos){
					r_size = param_p->buf_size;
				}else{
					r_size = w_pos - r_pos;
				}
			}else{
				if( param_p->ring_bufsize < r_pos + param_p->buf_size){
					top_move = 1;
					r_size = param_p->ring_bufsize - r_pos;
				}else{
					top_move = 0;
					r_size = param_p->buf_size;
				}
			}
			buf_p = param_p->buf + r_pos;

			// 2023.01.23 ADD START 結果出力スレッド(送信用)に計測開始を知らせる
			if( param_p->is_run_calc_s != STS_CALC_MEASURING ){
				param_p->is_run_calc_s = STS_CALC_START;
			}
			// 2023.01.23 ADD END   結果出力スレッド(送信用)に計測開始を知らせる

			// 送信
			w_size = send(param_p->sock_cli, buf_p, r_size, 0 );
			errno_bk = errno;
			if ( param_p->dbg_mode2 == 1 ){
				DBG(param_p,"send data(send_size=%ld, w_size=%ld)\n",  r_size, w_size );
			}
			if (w_size < 0) {
				if( errno_bk==EWOULDBLOCK || errno_bk==EAGAIN ){
					if( wait_for_socket(param_p, param_p->sock_cli, 10000, 1, 0) == 1){
						continue;
					}else{
						ERR(param_p,"send timeout\n");
					}
				}else{
					ERR(param_p,"send error\n" );
				}
				close(param_p->sock_cli);
				param_p->sock_cli=0;
				if( param_p->buf != NULL ){ free(param_p->buf); param_p->buf = NULL; }
				err_flg = 1;
				break;
			}
			s_interval_size += w_size;
			param_p->send_total += w_size;

			// リングバッファの読み込みポインタを進める
			if( top_move == 1 ){
				param_p->trans_r_pos = 0;
			}else{
				r_pos += w_size;
				if( param_p->ring_bufsize <= r_pos ){
					param_p->trans_r_pos = 0;
				}else{
					param_p->trans_r_pos = r_pos;
				}
			}
		}
		nanosleep(&tran_send_req, NULL);
	}

	LOG(param_p,"Relay node transport end\n");
	LOG(param_p, "recv_total_size %ld send_total_size %ld\n", param_p->recv_total, param_p->send_total);
	
	// 2023.01.23 ADD START 結果出力スレッド(送信用)が計測を開始しているか確認する
	if( 0 < param_p->send_total ){
		// 1byteでも送信されていれば、結果出力スレッドが開始されているか確認し
		// 開始されていなければ、開始されるのを待ってから終了処理を行うようにする
		while( param_p->is_run_calc_s != STS_CALC_MEASURING ){
			usleep(0);
		}
	}
	// 2023.01.23 ADD END   結果出力スレッド(送信用)が計測を開始しているか確認する

	// 結果出力スレッド(送信用)に終了指示を行う
	calc_stop(param_p, 0);

	DBG(param_p, "thread send end\n" );
	return NULL;
}



//*****************************************************************************
// MODULE	:	thread_save
// ABSTRACT	:	データ保存処理
// FUNCTION	:
// ARGUMENTS:	void*	p		(In) 引数のポインタ
// RETURN	:	0固定
// NOTE		:
//*****************************************************************************
void* thread_save(void* p)
{
	thread_param_t*	param_p = (thread_param_t*)p;								// パラメータ格納変数
	ssize_t				w_size;													// 送信データサイズ(一時格納用)
	ssize_t				s_interval_size = 0;									// 周期単位の送信サイズ
	ssize_t				r_size;													// 受信データサイズ(一時格納用)
	char*				buf_p;													// バッファのポインタ(一時格納用)
	int					w_pos;													// バッファ書き込みポインタ
	int					r_pos;													// バッファ読み込みポインタ
	int					top_move;												// リングバッファ先頭移動フラグ
	struct	timespec	req ={ 0, 1 };											// タスクスイッチ用sleep変数(1ns)
	int					err_flg=0;												// エラー発生中フラグ
	FILE*				input_fp = NULL;										// 外部入力ファイルポインタ
	FILE*				save_fp = NULL;											// 受信データ保存ファイルポインタ
	size_t				fwb;													// ファイル書き込み結果格納変数

	DBG(param_p, "thread save start\n" );
	param_p->is_run_save = 1;

	// アプリケーションモードが指定されている場合
	if( param_p->app_mode == 1 ){
		// 外部入力ファイルをオープンする
		input_fp = fopen(param_p->input_filename, "wb");
		if (input_fp == NULL) {
			ERR(param_p, "Can't open file %s\n", param_p->input_filename );
			close(param_p->sock_svr);
			param_p->sock_svr = 0;
			goto QUIT_RTN;
		}
	}

	// 受信データ保存指定されている場合
	if( strlen( param_p->save_filename ) != 0 ){
		// 受信データ保存ファイルをオープンする
		save_fp = fopen(param_p->save_filename, "wb");
		if (save_fp == NULL) {
			ERR(param_p, "Can't open file %s\n", param_p->save_filename );
			close(param_p->sock_svr);
			param_p->sock_svr = 0;
			goto QUIT_RTN;
		}
	}

	// 保存処理
	while( param_p->error_flg == 0 ){
		// エラーが発生したら処理を終了する
		if( err_flg==1 ) break;

		// 別スレッドから終了要求を受けた場合、保存処理を終了する
		if( param_p->is_run_save == 0 ){
			if( param_p->recv_total == param_p->save_total ){
				break;
			}
		}

		// リングバッファに保存するデータが存在するかチェックする
		while( (param_p->save_r_pos != param_p->save_w_pos) && param_p->error_flg == 0 ){
			w_pos = param_p->save_w_pos;
			r_pos = param_p->save_r_pos;

			// 書き込みサイズ算出
			if( w_pos < r_pos ){
				r_size = param_p->ring_bufsize - r_pos;
				top_move = 1;
			}else{
				r_size = w_pos - r_pos;
				top_move = 0;
			}
			buf_p = param_p->buf + r_pos;
			w_size = r_size;

			// データ書き込み（外部入力ファイル）
			if( input_fp != NULL ){
				fwb = fwrite(buf_p, 1, r_size, input_fp);
				if (fwb < r_size) {
					ERR(param_p, "Failed to write: %s\n", param_p->input_filename);
					err_flg=1;
					close(param_p->sock_svr);
					param_p->sock_svr = 0;
					break;
				}
			}

			// データ書き込み(受信データ保存ファイル)
			if( save_fp != NULL ){
				fwb = fwrite(buf_p, 1, r_size, save_fp);
				if (fwb < r_size) {
					ERR(param_p, "Failed to write: %s\n", param_p->save_filename);
					err_flg=1;
					close(param_p->sock_svr);
					param_p->sock_svr = 0;
					break;
				}
			}
			s_interval_size += w_size;
			param_p->save_total += w_size;

			// リングバッファの読み込みポインタを進める
			if( top_move == 1 ){
				param_p->save_r_pos = 0;
			}else{
				r_pos += w_size;
				if( param_p->ring_bufsize <= r_pos ){
					param_p->save_r_pos = 0;
				}else{
					param_p->save_r_pos = r_pos;
				}
			}
			nanosleep(&req, NULL);
		}
		nanosleep(&req, NULL);
	}

QUIT_RTN:
	// ファイルクローズ(外部入力ファイル)
	if( input_fp != NULL ){
		fclose(input_fp);
	}

	// ファイルクローズ(受信データ保存ファイル)
	if( save_fp != NULL ){
		fclose(save_fp);
	}

	DBG(param_p, "thread save end\n" );
	return NULL;
}

//*****************************************************************************
// MODULE	:	termination_notice_send_func
// ABSTRACT	:	終了通知送信処理
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	0 固定
// NOTE		:  
//*****************************************************************************
int termination_notice_send_func(thread_param_t* param_p)
{
	struct sockaddr_in	server;													// サーバーソケット情報格納変数
	ssize_t 			w_size;													// 送信データサイズ一時格納用
	size_t				send_size;												// 送信サイズ
	long long			lest_size = 0;											// 残り送信サイズ
	FILE*				output_fp = NULL;										// 外部出力ファイルポインタ
	ssize_t				s_interval_size = 0;									// 周期単位の送信サイズ
	socklen_t			optlen;													// オプション設定値長
	int					sendbuff;												// カーネルの送信バッファサイズ
	struct timeval		tv = {};												// 送信タイマー値
	int					errno_bk;												// エラー番号(退避用)
	int					connect_cnt = 0;										// エラー番号(退避用)
	
	// バッファの確保
	param_p->buf = malloc( 1 );
	if( param_p->buf == (char*)NULL ){
		ERR(param_p,"Out of memory\n" );
		goto QUIT_RTN;
	}
	memset( param_p->buf, 0x06, 1 );

	// ソケットの準備
	param_p->sock_cli = socket(AF_INET, SOCK_STREAM, 0);
	if (param_p->sock_cli < 0) {
	 	ERR(param_p,"socket create error\n");
		goto QUIT_RTN;
	}

	// 送信先のIPアドレスとポート番号を設定
	server.sin_family = AF_INET;
	server.sin_port = htons(param_p->notice_port);
	server.sin_addr.s_addr = inet_addr( param_p->notice_ip );
	if (server.sin_addr.s_addr == 0xffffffff) {
	 	ERR(param_p,"inet_addr error notice_ip=%s\n", param_p->notice_ip);
		goto QUIT_RTN;
	}
#ifndef __APPLE__
	//ソケットのtcp_congestionの設定
	if( param_p->congestion_mode == 1 ){
		if(setsockopt(param_p->sock_cli, IPPROTO_TCP, TCP_CONGESTION, param_p->congestion_name, strlen(param_p->congestion_name)) == -1){
		 	ERR(param_p,"setsockopt error (TCP_CONGESTION)\n");
			goto QUIT_RTN;
		}
	}
#endif
	// コネクト
	while(1){
		if (connect(param_p->sock_cli,(struct sockaddr *)&server, sizeof(server)) != 0) {
			if(connect_cnt > param_p->timeout * 1000000){
				ERR(param_p,"connect error notice_ip=%s, port=%d\n", param_p->notice_ip, param_p->notice_port  );
				goto QUIT_RTN;
			}
			connect_cnt++;
		}else{
			break;
		}
		usleep(1);
	}

	// 送信開始時刻になるまで Wait
	start_wait( param_p->send_start_time );

	// カーネルの送信バッファの設定
	optlen = sizeof(sendbuff);
	sendbuff = param_p->snd_bufsize;
	if( setsockopt(param_p->sock_cli, SOL_SOCKET, SO_SNDBUF, (void *)&sendbuff, optlen) == -1 ){;
		ERR(param_p,"SO_SNDBUF set error notice_ip=%s, port=%d\n", param_p->notice_ip, param_p->notice_port  );
		goto QUIT_RTN;
	}

	// 送信タイマーの設定
	tv.tv_sec=param_p->timeout;
    optlen=sizeof(tv);
	if(setsockopt(param_p->sock_cli,SOL_SOCKET,SO_SNDTIMEO,&tv,optlen)==-1){
		ERR(param_p,"SO_SNDTIMEO set error notice_ip=%s, port=%d\n", param_p->notice_ip, param_p->notice_port  );
		goto QUIT_RTN;
	}

	lest_size = 1;

	// 送信サイズが 0 byteならエラーメッセージを表示して終了する
	if( lest_size == 0 ){
		ERR(param_p, "output file size zero <%s>\n", param_p->output_filename );
		goto QUIT_RTN;
	}

	// 送信処理
	DBG(param_p,"termination notice send start\n" );
	while( (0 < lest_size) && param_p->error_flg == 0 ){

		// 1送信のパケットサイズを算出
		if( 1 <= lest_size ){
			send_size = 1;
		}else{
			send_size = lest_size;
		}

		// 2023.01.23 ADD START 結果出力スレッド(送信用)に計測開始を知らせる
		if( param_p->is_run_calc_s != STS_CALC_MEASURING ){
			param_p->is_run_calc_s = STS_CALC_START;
		}
		// 2023.01.23 ADD END   結果出力スレッド(送信用)に計測開始を知らせる

		// 送信処理
		w_size = send(param_p->sock_cli, param_p->buf, 1, 0 );
		errno_bk = errno;
		if ( param_p->dbg_mode2 == 1 ){
			DBG(param_p,"send data(send_size=%ld, w_size=%ld, errno=%d)\n",  send_size, w_size, errno_bk );
		}
		if (w_size < 0) {
			if( errno_bk==EWOULDBLOCK || errno_bk==EAGAIN ){
				ERR(param_p,"send timeout\n");
			}else{
				ERR(param_p,"send error\n");
			}
			break;
		}
		lest_size -= w_size;
		s_interval_size += w_size;
		param_p->send_total += w_size;
	}

QUIT_RTN:
	// 2023.01.23 ADD START 結果出力スレッド(送信用)が計測を開始しているか確認する
	if( 0 < param_p->send_total ){
		// 1byteでも送信されていれば、結果出力スレッドが開始されているか確認し
		// 開始されていなければ、開始されるのを待ってから終了処理を行うようにする
		while( param_p->is_run_calc_s != STS_CALC_MEASURING ){
			usleep(0);
		}
	}
	// 2023.01.23 ADD END   結果出力スレッド(送信用)が計測を開始しているか確認する

	// 結果出力スレッド(送信用)に終了指示を行う
	calc_stop(param_p, 0);

	DBG(param_p,"termination notice send end send_total = %ld\n", param_p->send_total );

	// ソケットをShutdownする
	shutdown( param_p->sock_cli, SHUT_WR );

	// 結果出力スレッド起動中フラグをOFFにする(送信計測用)
	if( param_p->is_run_calc_s == STS_CALC_WAIT ){
		param_p->is_run_calc_s = STS_CALC_STOP;
	}

	// ソケットをクローズする
	if( 0 < param_p->sock_cli ){
		close(param_p->sock_cli);
		param_p->sock_cli = 0;
	}

	// バッファを解放する
	if( param_p->buf != NULL ){
		free(param_p->buf); param_p->buf = NULL;
	}

	// 出力結果ファイルがオープン中であればクローズする
	if( output_fp != NULL ){
		fclose(output_fp);
		output_fp = NULL;
	}

	return 0;
}


//*****************************************************************************
// MODULE	:	termination_notice_recv_func
// ABSTRACT	:	終了通知受信処理
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) 引数のポインタ
// RETURN	:	0固定
// NOTE		:
//*****************************************************************************
int termination_notice_recv_func(thread_param_t* param_p)
{
	struct sockaddr_in	addr;													// 受信待ちソケット情報格納変数
	struct sockaddr_in	client;													// クライアントソケット情報格納変数
	socklen_t			len;													// クライアントソケット情報長
	ssize_t				r_size;													// 受信データサイズ(一時格納用)
	size_t				recv_size;												// 受信データサイズ
	fd_set				readfds;												// 受信ファイルディスクプリタ(読み込み用)
	fd_set				fds;													// 受信ファイルディスクプリタ(汎用)
	int					ret;													// 関数戻り値
	bool				yes = 1;												// オプション設定値
	ssize_t				r_interval_size = 0;									// 周期単位の受信サイズ
	int					recvbuff;												// カーネルの受信バッファサイズ
	socklen_t			optlen;													// オプション設定値長
	struct timeval		tv = {};												// 受信タイマー値
	int					errno_bk;												// エラー番号(退避用)

	// バッファの確保
	param_p->buf = malloc( 1 );
	if( param_p->buf == (char*)NULL ){
		ERR(param_p, "Out of memory\n" );
		goto QUIT_RTN;
	}
	memset( param_p->buf, 0, 1 );

	// ソケットの準備
	param_p->sock_acc = socket(AF_INET, SOCK_STREAM, 0);
	if (param_p->sock_acc < 0) {
	 	ERR(param_p,"socket create error\n");
		goto QUIT_RTN;
	}

	// ソケットオプションの設定
 	setsockopt(param_p->sock_acc, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

	// バインド
	while( param_p->error_flg == 0 ){
		addr.sin_family = AF_INET;
		addr.sin_port = htons(param_p->notice_port);
		addr.sin_addr.s_addr = INADDR_ANY;
		if (bind(param_p->sock_acc, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
//			if( param_p->auto_port == 1 ){
				param_p->notice_port ++;
				continue;
//			}
//			ERR(param_p,"bind error port=%d\n", param_p->notice_port);
//			goto QUIT_RTN;
		}
		break;
	}

	// リッスン
	if (listen(param_p->sock_acc, 5) != 0) {
		ERR(param_p,"listen error port=%d\n", param_p->notice_port);
		goto QUIT_RTN;
	}

	// 接続待ち(アクセプト)
	DBG(param_p,"accept wait... port=%d\n", param_p->notice_port);
	len = sizeof(client);
	param_p->sock_svr = accept(param_p->sock_acc, (struct sockaddr *)&client, &len);
	if (param_p->sock_svr < 0) {
		ERR(param_p,"accept error\n");
		goto QUIT_RTN;
	}
	DBG(param_p,"accept OK\n");
	FD_ZERO( &readfds );
	FD_SET( param_p->sock_svr, &readfds );

	// 受信待ち（recv）を行う
	//   受信開始タイミングを得るため、recv待ちではなくselectで待つ
	while( param_p->error_flg == 0 ){
		DBG(param_p,"recv wait... (socket=%d)\n", param_p->sock_svr );
		memcpy(&fds, &readfds, sizeof(fd_set));
		ret = select((int)(param_p->sock_svr + 1), &fds, NULL, NULL, NULL);
		if (ret <= 0) {
			if( ret == 0 ){
				continue;
			}
			ERR(param_p, "select error\n");
			goto QUIT_RTN;
		}
		break;
	}

	// カーネルの受信バッファの設定
	optlen = sizeof(recvbuff);
	recvbuff = param_p->rcv_bufsize;
	if( setsockopt(param_p->sock_svr, SOL_SOCKET, SO_RCVBUF, (void *)&recvbuff, optlen)==-1){
		ERR(param_p,"SO_RCVBUF set error ip=%s, port=%d\n", param_p->ip, param_p->notice_port  );
		goto QUIT_RTN;
	}

	// 受信タイマーの設定
	tv.tv_sec=param_p->timeout;
    optlen=sizeof(tv);
	if(setsockopt(param_p->sock_svr,SOL_SOCKET,SO_RCVTIMEO,&tv,optlen)==-1){
		ERR(param_p,"SO_RCVTIMEO set error ip=%s, port=%d\n", param_p->ip, param_p->notice_port  );
		goto QUIT_RTN;
	}

	DBG(param_p,"termination notice recv start (select value=%d)\n", ret);

	// 受信処理
	recv_size = 1;
	param_p->recv_total = 0;
	while( param_p->error_flg == 0 ){

		// 2023.01.23 ADD START 結果出力スレッド(受信用)に計測開始を知らせる
		if( param_p->is_run_calc_r != STS_CALC_MEASURING ){
			param_p->is_run_calc_r = STS_CALC_START;
		}
		// 2023.01.23 ADD END   結果出力スレッド(受信用)に計測開始を知らせる

		r_size = recv(param_p->sock_svr, &param_p->buf[0], recv_size, 0);

		errno_bk=errno;
		if ( param_p->dbg_mode2 == 1 ){
			DBG(param_p,"termination notice recv data(recv_size=%ld, r_size=%ld)\n",  recv_size, r_size );
		}
		if (r_size < 1) {
			if( r_size != 0 ){
				if( errno_bk==EWOULDBLOCK || errno_bk==EAGAIN ){
					ERR(param_p,"recv timeout\n");
				}else{
					ERR(param_p,"recv error\n");
				}
			}
			DBG(param_p,"recv stop\n");

			close(param_p->sock_svr);
			param_p->sock_svr = 0;
			break;
		}
		r_interval_size += r_size;
		param_p->recv_total += r_size;


	}
	LOG(param_p,"Termination notice ip = %s, port = %d\n", inet_ntoa(client.sin_addr), param_p->notice_port );

QUIT_RTN:

	// 2023.01.23 ADD START 結果出力スレッド(受信用)が計測を開始しているか確認する
	if( 0 < param_p->recv_total ){
		// 1byteでも送信されていれば、結果出力スレッドが開始されているか確認し
		// 開始されていなければ、開始されるのを待ってから終了処理を行うようにする
		while( param_p->is_run_calc_r != STS_CALC_MEASURING ){
			usleep(0);
		}
	}
	// 2023.01.23 ADD END   結果出力スレッド(受信用)が計測を開始しているか確認する

	// 結果出力スレッド(受信用)に終了指示を行う
	calc_stop(param_p, 1);

	DBG(param_p,"termination notice recv end recv_total=%ld\n", param_p->recv_total );

	// 結果出力スレッド起動中フラグをOFFにする(受信計測用)
	if( param_p->is_run_calc_r == STS_CALC_WAIT ){
		param_p->is_run_calc_r = STS_CALC_STOP;
	}

	// バッファを解放する
	if( param_p->buf != NULL ){
		free(param_p->buf);
		param_p->buf = NULL;
	}

	// ソケットをクローズする(アクセプト用)
	if( 0 < param_p->sock_acc ){
		close(param_p->sock_acc);
		param_p->sock_acc = 0;
	}

	// ソケットをクローズする(受信用)
	if( 0 < param_p->sock_svr ){
		close(param_p->sock_svr);
		param_p->sock_svr = 0;
	}
	return 0;
}