//*****************************************************************************
// FILENAM	:	common.c
// ABSTRAC	:	共通処理
// NOTE		:	
//*****************************************************************************
#include	"tcpTester.h"


//*****************************************************************************
// MODULE    : token_analize
// ABSTRACT  : 文字列分解処理
// FUNCTION  : 指定された文字列をカンマ区切りで分割し、項目情報配列に格納する
// ARGUMENTS : char*		text	(In)	元文字列ポインタ
//           : char*       	sep     (In)	セパレータ文字
//           : char        	sep     (In)	セパレータ文字
//           : ITEM_ARY_T*	ary		(Out)	分解後の文字列格納配列ポインタ
// RETURN    :  0 = 分解された項目数
//           : -1 = 分解未実施
// NOTE      :
//*****************************************************************************
int token_analize( char* text, char* sep, ITEM_ARY_T* ary )
{
	char*	p;																	// 汎用ポインタ
	int		item = 0;															// 項目数

	// 先頭項目の抽出
	p = strtok(text,sep);

	// コメント行なら何もしないで関数リターン
	if(p[0]=='#') return(-1);
	strncpy( ary[item].data, p, (ITEM_LENGTH_MAX-1) );
	item++;

	// ２番目以降のアイテム抽出
	while(p!=NULL){
		p = strtok(NULL,sep);
		if(p!=NULL){
			// 改行文字を削除
			lntrim(p);
			// 取得したアイテムを配列に格納する
			strncpy( ary[item].data, p, (ITEM_LENGTH_MAX-1) );
			item++;
		}
    }
	return(item);
}


//*****************************************************************************
// MODULE    : lntrim
// ABSTRACT  : 改行文字NULL置換
// FUNCTION  : 終端文字列の改行文字をNULLに置き換える
// ARGUMENTS : char* str (In/Out) 置き換え対象文字列
// RETURN    : なし
// NOTE      :
//*****************************************************************************
void lntrim(char* str)
{
	char *p;																	// 汎用ポインタ

	// 改行(\n)とキャリッジリターン(\r)をNULL文字に置き換える
	p = strchr(str, '\n');	if(p != NULL) *p = '\0';
	p = strchr(str, '\r');	if(p != NULL) *p = '\0';
}


//*****************************************************************************
// MODULE    : is_proc_running
// ABSTRACT  : プロセス起動中確認
// FUNCTION  : 
// ARGUMENTS : char* proc_name (In) プロセス名
// RETURN    : -1 : プロセスの起動確認に失敗
//           :  0 : 起動中のプロセス無(自プロセスは含まない)
//           :  1 : 起動中のプロセス有
// NOTE      :
//*****************************************************************************
int is_proc_running(char* proc_name)
{
	char		buf[LINE_BUF_MAX];												// 汎用バッファ
	char		cmdline[LINE_BUF_MAX];											// コマンド格納変数
	ITEM_ARY_T	ary[LINE_ITEM_MAX];												// トークンを格納する配列
	int			items;															// トークン数
	FILE*		fp = NULL;														// コマンド実行用ファイルポインタ
	int			found;															// 結果格納用変数
	pid_t		my_pid;															// 自プロセスID

	// 自プロセスIDを取得
	my_pid = getpid();

	// コマンド実行（自プロセスが起動中か？)
	sprintf( cmdline, "ps -ef | grep %s | grep daemon | grep -v %d | grep -v grep | grep -v \"sh -c\" ", proc_name, my_pid );
	if ( (fp=popen(cmdline,"r")) ==NULL) {
		return(-1);
	}

	// コマンド実行結果を解析
	found = 0;
	memset( ary, 0, sizeof(ary) );												// 配列初期化
	while(fgets(buf, LINE_BUF_MAX, fp) != NULL) {
		items = token_analize( buf, " ,", ary );
		if( items > 2 ){
			found = 1;
			break;
		}
	}
	(void) pclose(fp);

	return( found );
}


//*****************************************************************************
// MODULE    : exec_cmd
// ABSTRACT  : プロセス起動中確認
// FUNCTION  : 
// ARGUMENTS : char* proc_name (In) プロセス名
// RETURN    : -1 : プロセスの起動確認に失敗
//           :  0 : 起動中のプロセス無(自プロセスは含まない)
//           :  1 : 起動中のプロセス有
// NOTE      :
//*****************************************************************************
int exec_command(char* cmdline, ITEM_ARY_T* ary)
{
	char		buf[LINE_BUF_MAX];												// 汎用バッファ
	int			items = 0;														// トークン数
	FILE*		fp = NULL;														// コマンド実行用ファイルポインタ

	// コマンド実行（自プロセスが起動中か？)
	if ( (fp=popen(cmdline,"r")) ==NULL) {
		return(-1);
	}

	// コマンド実行結果を解析
	while(fgets(buf, LINE_BUF_MAX, fp) != NULL) {
		items = token_analize( buf, " \t", ary );
		if( items > 2 ){
			break;
		}
	}
	(void) pclose(fp);

	return( items );
}


//*****************************************************************************
// MODULE    : get_free_thread_no
// ABSTRACT  : 空きスレッド番号取得
// FUNCTION  : スレッド管理テーブルを検索し、未使用のスレッド番号を取得する
// ARGUMENTS : なし
// RETURN    : -1		:	空きスレッド番号なし
//           : -1以外	:	空きスレッド番号
// NOTE      :
//*****************************************************************************
thread_param_t* regist_thread(thread_param_t* param_p)
{
	int	ii;																		// 汎用Index
	int	thread_no = -1;															// 退避用スレッド番号

	// 最終スレッド番号が指し示すスレッド管理テーブルを確認し
	// 未使用状態か確認する
	if( g_thread_mngtbl[g_thread_no].used_flg == 0 ){

		// 未使用であれば、使用中フラグをONにしてスレッド管理テーブルの情報を取得する
		g_thread_mngtbl[g_thread_no].used_flg = 1;
		param_p->thread_no = g_thread_no;
		memcpy( &g_thread_mngtbl[g_thread_no].param, param_p, sizeof(thread_param_t) );

		// 最終スレッド番号をカウントアップする
		g_thread_no ++;
		if( THREAD_MAX <= g_thread_no ){
			g_thread_no = 0;
		}
		thread_no = param_p->thread_no;
		DBG(param_p, "thread no => %d\n", thread_no );
	}

	// 最終スレッド番号が指し示すスレッド管理テーブルを確認した結果、使用中だった場合
	if( thread_no == -1 ){
		// スレッド管理テーブルを先頭から検索し、未使用のスレッド番号を検索する
		for( ii=0; ii<THREAD_MAX; ii++ ){

			// 使用中フラグがOFF(=未使用)か？
			if( g_thread_mngtbl[ii].used_flg == 0 ){

				// 使用中フラグをONにする
				g_thread_mngtbl[ii].used_flg = 1;
				param_p->thread_no = ii;
				memcpy( &g_thread_mngtbl[ii].param, param_p, sizeof(thread_param_t) );
				thread_no = ii;
				break;
			}
		}
	}

	// 空きスレッド番号が見つからなかった場合は、NULLを返す
	if( thread_no == -1 ){
		return( NULL );
	}

	return &g_thread_mngtbl[thread_no].param;
}


//*****************************************************************************
// MODULE    : set_free_thread_no
// ABSTRACT  : スレッド番号解放
// FUNCTION  : 指定されたスレッド管理テーブルの対象スレッドを解放状態にする
// ARGUMENTS : int	thread_no		スレッド番号
// RETURN    : 0  : 正常終了
//           : -1 : 異常終了(解放状態へ更新失敗)
// NOTE      :
//*****************************************************************************
int release_thread(thread_param_t* param_p)
{
	int thread_no = param_p->thread_no;											// スレッド番号

	// スレッド管理テーブルをクリアする
	memset( &g_thread_mngtbl[thread_no].param, 0, sizeof(thread_param_t) );
	g_thread_mngtbl[thread_no].used_flg = 0;

	return 0;
}


//*****************************************************************************
// MODULE	: set_start_time
// ABSTRACT	: 送信開始時刻の通算秒取得
// FUNCTION	: 引数で受け取った文字列(HH:MM:SS)から開始時刻(通算秒を取得する)
// ARGUMENTS: なし
// RETURN	: なし
// NOTE		:
//*****************************************************************************
time_t set_start_time( char* str_time )
{
	time_t		t;																// 退避用時刻格納用変数
	time_t		result_time;													// 返却用時刻格納変数
    struct	tm	time_st;														// 編集用用時刻格納用変数
	size_t		len = strlen(str_time);											// 時刻文字列長
	char		buf[4];

	// 時刻文字列が送信開始時刻=0(即時送信)とする
	if( len == 0 ){
		return 0;
	}
	// 文字列が8文字以下であれば、エラー出力し即時送信(=0)を返す
	if( len < 8 ){
		ERR( NULL, "starttime format error <%s> (format=hh:mm:ss)\n", str_time );
		return 0;
	}

	// 時刻文字列を通算秒に変換する
    t = time(NULL);
    memcpy( &time_st, localtime(&t), sizeof(time_st) );;
	memset(buf,0,sizeof(buf));	memcpy(buf,&str_time[0],2);	time_st.tm_hour	= atoi(buf);	// 時
	memset(buf,0,sizeof(buf));	memcpy(buf,&str_time[3],2);	time_st.tm_min	= atoi(buf);	// 分
	memset(buf,0,sizeof(buf));	memcpy(buf,&str_time[6],2);	time_st.tm_sec	= atoi(buf);	// 秒
	result_time = mktime(&time_st);
	if( result_time < t ){
		result_time += 3600 * 24;
	}

	// 通算秒を返却する
	return result_time;
}


//*****************************************************************************
// MODULE    : start_wait
// ABSTRACT  : 指定時刻までWait
// FUNCTION  : 指定時刻になるまで待つ(sleepする)
// ARGUMENTS : char*	in		(In)	バイナリイメージテキスト格納バッファアドレス
//           : char*	out		(Out)	バイナリデータ格納バッファアドレス
//           : size_t*	size	(Out)	バイナリデータサイズ
// RETURN    : なし
// NOTE      :
//*****************************************************************************
void start_wait( time_t t )
{
	struct	timeval	_time;													// 時刻構造体
	long			rest;													// 残り時間
	long			rest_back = -99999999;									// 残り時間(退避用)

	// 指定時刻が0ならWaitせずに処理を終了する（即時送信)
	if( t ==  0 ) return;

	// 現在時刻と指定時刻の通算秒を比較し、指定時刻になるまでsleepする
	while( 1 ){
		gettimeofday(&_time, NULL);
		rest = (long)difftime(t, _time.tv_sec);
		if( rest <= 0 ) break;
		if( rest != rest_back ){
			DSP( "\r開始まであと %5ld秒", rest );
			fflush(stdout);
			rest_back = rest;
		}
		usleep( 1000 * 100 );
	}
}


//*****************************************************************************
// MODULE    : is_dir_filepath
// ABSTRACT  : ディレクトリ判定処理
// FUNCTION  : 指定ファイル名がディレクトリ付きか判定する
// ARGUMENTS : char*	filepass		(In)	ファイルパス
// RETURN    : true  = ディレクトリ付き
//           : false = ディレクトリ無し
// NOTE      :
//*****************************************************************************
bool is_dir_filepath( char* filepath )
{
	char work[256];															// ワークエリア

	// 指定ファイル名にディレクトリが付加されているかチェックする
	strcpy( work, filepath );
	if( strcmp( dirname(work), "." ) == 0 ){
		// ディレクトリ無
		return false;
	}
	// ディレクトリ有
	return true;
}

//*****************************************************************************
// MODULE    : is_ext_filepath
// ABSTRACT  : 拡張子判定処理
// FUNCTION  : 指定ファイル名が拡張子付きか判定する
// ARGUMENTS : char*	filepass		(In)	ファイルパス
// RETURN    : true  = 拡張子付き
//           : false = 拡張子無し
// NOTE      :
//*****************************************************************************
bool is_ext_filepath( char* filepath )
{
	char work[256];															// ワークエリア

	// 指定ファイル名に拡張子が付加されているかチェックする
	strcpy( work, filepath );
	if( strrchr(work, '.') == 0 ){
		// 拡張子無
		return false;
	}
	// 拡張子有
	return true;
}

