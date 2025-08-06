//*****************************************************************************
// FILENAM	:	init.c
// ABSTRAC	:	初期処理
// NOTE		:	
//*****************************************************************************
#include	"tcpTester.h"


//*****************************************************************************
// MODULE	:	init
// ABSTRACT	:	初期処理
// FUNCTION	:
// ARGUMENTS:	INT    argc   (In) 引数の数
//			:	CHAR** argv   (In) 引数のポインタリスト
// RETURN	:	 0 = 正常終了
//			:	-1 = エラー発生
// NOTE		:
//*****************************************************************************
int	init(int argc, char** argv)
{
	struct option longopts[] = {												// オプションパラメータテーブル
		{	"souce",			required_argument,	NULL,	's'	},				// [-c]:クライアントモード(送信ノードプログラムとして起動)
		{	"relay",			required_argument,	NULL,	'r'	},				// [-r]:中継モード(中継ノードプログラムとして起動)
		{	"destination",		required_argument,	NULL,	'd'	},				// [-s]:サーバーモード(受信ノードプログラムとして起動)
		{	"quit",				no_argument,		NULL,	'Q'	},				// [-Q]:常駐プロセス終了
		{	"port",				required_argument,	NULL,	'p'	},				// [-p]:ポート番号(-c:送信先ポート番号, -s:受信待ポート番号, -r:送受信ポート番号)
		{	"size",				required_argument,	NULL,	'l'	},				// [-l]:送信データサイズ
		{	"starttime",		required_argument,	NULL,	't'	},				// [-t]:送信開始日時
		{	"bufsize",			required_argument,	NULL,	'b'	},				// [-b]:バッファサイズ
		{	"interval",			required_argument,	NULL,	'i'	},				// [-i]:結果出力インターバル
		{	"result_filename",	required_argument,	NULL,	'f'	},				// [-f]:結果出力ファイル名
		{	"help",				no_argument,		NULL,	'h'	},				// [-h]:ヘルプ
		{	"congestion",		required_argument,	NULL,	'C'	},				// [-C]:輻輳モード
		{	"app_mode",			required_argument,	NULL,	'A'	},				// [-A]:中継モード(0=トランスポート, 1=アプリケーション)
		{	"i_datname",		required_argument,	NULL,	'I'	},				// [-I]:外部プログラム用入力ファイル名
		{	"o_datname",		required_argument,	NULL,	'O'	},				// [-O]:外部プログラム用出力ファイル名
		{	"save_filename",	required_argument,	NULL,	'W'	},				// [-W]:受信データ保存ファイル名
		{	"daemon",			no_argument,		NULL,	'?'	},				// [--daemon]バックグラウンド起動 (long option only)
		{	"DEBUG",			no_argument,		NULL,	'?'	},				// [--DEBUG]モード (long option only)
		{	"DEBUG2",			no_argument,		NULL,	'?'	},				// [--DEBUG]モード2 (long option only)
		{	"auto_port",		required_argument,	NULL,	'P'	},				// [-P]:ポート選択
		{	"buffer_size",		required_argument,	NULL,	'B'	},				// [-B]:転送バッファサイズ(トランスポートレベル用)
		{	"SND_BUFSIZE",		required_argument,	NULL,	'S'	},				// [-S]:送信バッファサイズ
		{	"RCV_BUFSIZE",		required_argument,	NULL,	'R'	},				// [-R]:受信バッファサイズ
		{	"timeout",			required_argument,	NULL,	'T'	},				// [-T]:タイムアウト時間(s)
		{	"so",				required_argument,	NULL,	('s'<<8) + 'o'	},	// [--so]:送信ノードアウトバウンドIPアドレス
		{	"ro",				required_argument,	NULL,	('r'<<8) + 'o'	},	// [--ro]:中継モードアウトバウンドIPアドレス
		{	"do",				required_argument,	NULL,	('d'<<8) + 'o'	},	// [--do]:受信ノードアウトバウンドIPアドレス
		{	"sp",				required_argument,	NULL,	('s'<<8) + 'p'	},	// [--sp]:受信完了通知ポート番号（受信ノードのみ）
		{	0,					0,					0,		0	},
	};
	thread_param_t		thread_param = {};										// スレッドパラメータ構造体
	int 				opt;													// 見つかったオプション文字列
	int 				longindex;												// 見つかったオプションパラメータテーブルのIndex
	int					mode = 0;												// 起動モード(MODE_SEND=送信ノード, MODE_RECV=受信ノード, MODE_RELAY=中継ノード, MODE_QUIT=終了)
	int					is_run;													// 起動中確認フラグ(0=未起動, 1=起動中)
	int					ret;													// 汎用戻り値
	char				cmd[1024];												// コマンド格納用バッファ
	char				prg_path[256];											// プログラムパス名
	char				work[1024];												// ワークバッファ
	ITEM_ARY_T			ary[8] = {};											// コマンド返却値配列
	char				ip[32] = {};											// [-c or -r]:送信先IPアドレス
	char				node_ip[32] = {};										// [-so or -ro or -do]:アウトバウンドIPアドレス
	unsigned short		port = DEF_PORT_NO;										// [-p]:受信ポート番号(default=20000)
	char				notice_ip[32] = {};										// 受信完了通知先IPアドレス（受信ノードのみ）
	unsigned short		notice_port = DEF_NOTICE_PORT_NO;						// [--sp]:受信完了通知ポート番号（送信ノードのみ）(default=30000)
	size_t				size = DEF_SEND_SIZE;									// [-l];送信データサイズ(defalut=2G)
	char				sndtime[32] = {};										// [-t]:送信開始日時
	int					congestion_mode = 0;									// [-C]:輻輳モード(0=TCP, 1=CCP);
	char				congestion_name[32] = {};								//     :輻輳モード名( cubic(default), reno, ccp)
	int					app_mode = 0;											// [-A]:中継モード(0=アプリケーションレベル, 1=トランスポートレベル)
	char				app_filename[256+32] = {};								//     :外部プログラム名
	int					interval = 1;											// [-i]:結果出力インターバル(s)
	char				input_filename[256+32] = { DEF_INPUT_NAME };			// [-I]:外部プログラム用入力ファイル名
	char				output_filename[256+32] = { DEF_OUTPUT_NAME };			// [-O]:外部プログラム用出力ファイル名
	size_t				buf_size = DEF_BUF_SIZE;								// [-b]:バッファサイズ(defalut=128Kbyte)
	int					help_mode = 0;											// [-h]:ヘルプモード
	int					daemon_mode=0;											// [--debug]:daemonモード
	char				save_filename[256+32];									// [-W]:受信データ保存ファイル名
	int					auto_port = 1;											// [-P]:自動ポート選択
	int					rcv_bufsize = 0;										// [-R]:受信バッファサイズ
	int					snd_bufsize = 0;										// [-S]:送信バッファサイズ
	int					ring_bufsize = DEF_RNG_BUF_SIZE;						// [-B]:リングバッファサイズ(リング)
	int					timeout=DEF_TIMEOUT;									// [-T]:タイムアウト時間
	int					option_error = 0;

	// 自プロセス情報の保持
#ifdef __APPLE__
    uint32_t prg_path_size = sizeof(prg_path) ;
    _NSGetExecutablePath(prg_path, &prg_path_size);
#else
	readlink( "/proc/self/exe", prg_path, sizeof(prg_path) );
#endif
	strcpy( g_prc_name, basename(prg_path) );
	strcpy( g_prc_path, dirname(prg_path) );

	// カーネルの最大送信バッファの値を取得する
#ifdef __APPLE__
    if( exec_command( "sysctl -n net.inet.tcp.sendspace", ary ) < 1 ){
        ERR( NULL, "command errro (sysctl -n net.inet.tcp.sendspace)\n" );
        return(-1);
    };
    snd_bufsize = atoi(ary[0].data);
#else
	if( exec_command( "cat /proc/sys/net/ipv4/tcp_wmem", ary ) < 3 ){
		ERR( NULL, "read errro (/proc/sys/net/ipv4/tcp_wmem)\n" );
		return(-1);
	};
	snd_bufsize = atoi(ary[1].data);
#endif

	// カーネルの最大受信バッファの値を取得する
#ifdef __APPLE__
    if( exec_command( "sysctl -n net.inet.tcp.recvspace", ary ) < 1 ){
        ERR( NULL, "command errro (sysctl -n net.inet.tcp.recvspace)\n" );
        return(-1);
    };
    rcv_bufsize = atoi(ary[0].data);
#else
	if( exec_command( "cat /proc/sys/net/ipv4/tcp_rmem", ary ) < 3 ){
		ERR( NULL, "read errro (/proc/sys/net/ipv4/tcp_rmem)\n" );
		return(-1);
	};
	// tabuchi 2024/12/23
	// defaultのバッファサイズを受信バッファとする
	rcv_bufsize = atoi(ary[1].data);
#endif
	
	// 引数の解析を行う
	while ((opt = getopt_long(argc, argv, "s:d:r:p:l:t:b:i:f:hC:A:I:O:QW:P:B:S:R:T:", longopts, &longindex)) != -1) {
		switch (opt) {
		case 's':	mode = MODE_SEND;	strcpy(ip, optarg);					break;
		case 'd':	mode = MODE_RECV;	strcpy(notice_ip, optarg);			break;
		case 'r':	mode = MODE_RELAY;	strcpy(ip, optarg);					break;
		case 'Q':	mode = MODE_QUIT;										break;
		case 'p':	if( optarg!=NULL ){ port = atoi(optarg); }				break;
		case 'l':	if( optarg!=NULL ){ size = atoll(optarg); }				break;
		case 't':	strcpy(sndtime, optarg);								break;
		case 'b':	if( optarg!=NULL ){ buf_size = atoll(optarg); }			break;
		case 'i':	if( optarg!=NULL ){ interval = atoi(optarg); }			break;
		case 'f':	strcpy(g_result_file, optarg);							break;
		case 'W':	strcpy(save_filename, optarg);							break;
		case 'h':	help_mode=1;											break;
		case 'C':	congestion_mode=1; strcpy(congestion_name, optarg); 	break;
		case 'A':	app_mode=1; strcpy(app_filename, optarg);				break;
		case 'I':	strcpy(input_filename, optarg);							break;
		case 'O':	strcpy(output_filename, optarg);						break;
		case 'P':	if( optarg!=NULL ){ auto_port = atoi(optarg); }			break;
		case 'B':	if( optarg!=NULL ){ ring_bufsize = atoi(optarg); }		break;
		case 'S':	if( optarg!=NULL ){ snd_bufsize = atoi(optarg); }		break;
		case 'R':	if( optarg!=NULL ){ rcv_bufsize = atoi(optarg); }		break;
		case 'T':	if( optarg!=NULL ){ timeout = atoi(optarg); }			break;
		case ('s'<<8) + 'o':	if(mode == MODE_SEND){	strcpy(node_ip, optarg);}		break;
		case ('d'<<8) + 'o':	if(mode == MODE_RECV){	strcpy(node_ip, optarg);}		break;
		case ('r'<<8) + 'o':	if(mode == MODE_RELAY){	strcpy(node_ip, optarg);}		break;
		case ('s'<<8) + 'p':	if(mode == MODE_SEND || mode == MODE_RECV){	notice_port = atoi(optarg);}		break;

		default:	// シークレットオプション (long option only)
					if( strncmp( argv[optind-1], "--daemon", strlen("--daemon" ) ) == 0 ){
						daemon_mode = 1;
					}else if( strncmp( argv[optind-1], "--DEBUG", strlen("--DEBUG" ) ) == 0 ){
						g_dbg_mode = 1;
						if( strncmp( argv[optind-1], "--DEBUG2", strlen("--DEBUG2" ) ) == 0 ){
							g_dbg_mode2 = 1;
						}
					}else{
						option_error = 1;
					}
					break;
		}
	}
	DBG(NULL,">>>>>>>> Start program >>>>>>>>\n");

	// オプションでヘルプ指定されている場合ヘルプ表示を行う
	if( help_mode == 1 ){
		disp_help( argc, argv );
		return(-1);
	}

	// 起動パラメータをパラメータに設定する
	//   デーモン起動でない場合のみ実行する
	//   デーモン起動(--daemon)は、ユーザーが指定するオプションではなく
	//   初回起動時に、本プログラム自身で常駐プログラムとして起動させる。
	//   デーモンとして起動されたプロセスは、常駐のみ行うためパラメータの保持は不要である。
	//   その後、デーモンプロセスは、データ送受信のパラメータをメッセージで受け取って動作する。
	if( daemon_mode == 0 ){

		// 起動モード(MODE_SEND=送信ノード, MODE_RECV=受信ノード, MODE_RELAY=中継ノード, MODE_QUIT=終了)
		thread_param.mode = mode;

		// 送信先IPアドレス
		strncpy( thread_param.ip, ip, sizeof(thread_param.ip) );

		// 通信ポート番号
		thread_param.port = port;

		// データサイズ
		thread_param.size = size;

		// バッファサイズ
		thread_param.buf_size = buf_size;

		// 輻輳モード(0=TCP, 1=CCP…後で文字列に変更するかも);
		thread_param.congestion_mode = congestion_mode;

		// 輻輳モード(0=TCP, 1=CCP…後で文字列に変更するかも);
		strncpy(thread_param.congestion_name,congestion_name,
									sizeof(thread_param.congestion_name) );

		// 中継モード(0=アプリケーションレベル, 1=トランスポートレベル)
		thread_param.app_mode = app_mode;

		// 送信開始時刻
		thread_param.send_start_time = set_start_time( sndtime );

		// 計測インターバル]
		thread_param.interval = interval;

		// [-P]:自動ポート選択
		thread_param.auto_port = auto_port;

		// [-B]:転送用バッファサイズ
		if( ring_bufsize < (thread_param.buf_size * 2 ) ){
			ERR( NULL, "ring buffer size error [%ld < ring_buffer_size]\n", (thread_param.buf_size * 2) );
			return(-1);
		}
		thread_param.ring_bufsize = ring_bufsize;


		// [-W]:送信バッファサイズ
		thread_param.snd_bufsize = snd_bufsize;

		// [-R]:受信バッファサイズ1
		thread_param.rcv_bufsize = rcv_bufsize;

		// [-T]:タイムアウト時間
		thread_param.timeout = timeout;

		// 結果出力ファイル名
		if( !is_dir_filepath( g_result_file ) ){
			memset( work, 0, sizeof(work) );
			strncpy( work, g_result_file, sizeof(work) - 1 );
			snprintf( g_result_file, sizeof(g_result_file), "%s/%s", g_prc_path, basename(work) );
		}
		strncpy(thread_param.result_filename, g_result_file, 
									sizeof(thread_param.result_filename) );

		// 受信データ保存ファイル名
		if( 0 < strlen( save_filename ) ){
			if( !is_dir_filepath( save_filename ) ){
				memset( work, 0, sizeof(work) );
				strncpy( work, save_filename, sizeof(work) - 1 );
				snprintf( save_filename, sizeof(save_filename), "%s/%s", g_prc_path, basename(work) );
			}
			strncpy(thread_param.save_filename, save_filename, 
									sizeof(thread_param.save_filename) );
		}

		// 外部プログラム名
		if( 0 < strlen( app_filename ) ){
			if( !is_dir_filepath( app_filename ) ){
				memset( work, 0, sizeof(work) );
				strncpy( work, app_filename, sizeof(work) - 1 );
				snprintf( app_filename, sizeof(app_filename), "%s/%s", g_prc_path, basename(work) );
			}
			strncpy( thread_param.app_filename, app_filename, 
										sizeof(thread_param.app_filename) );
		}

		// 外部プログラム用入力ファイル名
		if( !is_dir_filepath( input_filename ) ){
			memset( work, 0, sizeof(work) );
			strncpy( work, input_filename, sizeof(work) - 1 );
			snprintf( input_filename, sizeof(input_filename), "%s/%s", g_prc_path, basename(work) );
		}
		strncpy(thread_param.input_filename, input_filename, 
									sizeof(thread_param.input_filename) );

		// 外部プログラム用出力ファイル名
		if( !is_dir_filepath( output_filename ) ){
			memset( work, 0, sizeof(work) );
			strncpy( work, output_filename, sizeof(work) - 1 );
			snprintf( output_filename, sizeof(output_filename), "%s/%s", g_prc_path, basename(work) );
		}
		strncpy(thread_param.output_filename, output_filename, 
									sizeof(thread_param.output_filename) );

		// 受信完了通知先IPアドレス
		strncpy( thread_param.notice_ip, notice_ip, sizeof(thread_param.notice_ip) );

		// 受信完了通知先ポート番号
		thread_param.notice_port = notice_port;

		// デバッグモード
		thread_param.dbg_mode = g_dbg_mode;
		thread_param.dbg_mode2 = g_dbg_mode2;

		// 変数の内容をログ出力する (DEBUG用)
		deg_dsp_variable(&thread_param);
	}

	// 不正オプションが指定されていた場合、エラーメッセージを表示する
	if( option_error ){
		ERR(NULL, "Option error\n");

	// daemonモード(--daemon)の場合
	} else if( daemon_mode == 1 ){

		// プロセス起動状態をチェック
		is_run = is_proc_running( basename(argv[0]) );
		if( is_run == -1 ){
			ERR(NULL, "porcess check error\n" );
			DBG(NULL, "[%s](%d):>>>>>>>> End program >>>>>>>>\n", __FILE__, __LINE__);
			return(-1);
		}
		if( is_run == 1 ){														// 起動中の場合
			DBG(NULL, "already running...\n" );
			exit(0);
		}

		// メッセージキューをオープンする
		DBG(NULL,"[%s](%d):message que create\n", __FILE__, __LINE__);
#ifdef __APPLE__
        key_t que_key = ftok(g_prc_path, 'a');
        g_que = msgget(que_key,0666|IPC_CREAT);
        if(g_que == -1){
            ERR(NULL, "msgget error (%s)\n", strerror(errno));
            return(-1);
        }
#else
		g_que = mq_open(MSGQ_KEY, O_RDWR|O_CREAT, 0644, NULL);
		if(-1 == g_que){
			ERR(NULL, "mq_open error (%s)\n", strerror(errno));
			return(-1);
		}
		mq_getattr(g_que, &g_mq_attr);
#endif
		// デーモン化して処理を終了する
		if ((ret = daemon(0, 0)) != 0) {
			ERR(NULL, "daemon boot failure\n" );
			DBG(NULL, "[%s](%d):>>>>>>>> End program >>>>>>>>\n", __FILE__, __LINE__);
			return(-1);
		}
		return(0);																// 処理継続(メッセージdeque待ちへ)

	// 終了要求の場合
	} else if( mode == MODE_QUIT ) {
		// キューを削除する
#ifdef __APPLE__
#else
		mq_unlink(MSGQ_KEY);
#endif
		// プロセスを強制終了する
		snprintf( cmd, sizeof(cmd), "pkill -9 %s &> /dev/null", g_prc_name );
		system( cmd );
		exit(0);

	// 通常起動の場合
	} else {

		// プロセス起動状態をチェック
		is_run = is_proc_running( basename(argv[0]) );
		if( is_run == -1 ){
			ERR(NULL,"process check error\n" );
			DBG(NULL,"[%s](%d):>>>>>>>> End program >>>>>>>>\n", __FILE__, __LINE__);
			return(-1);
		}

		// 未起動の場合
		if( is_run == 0 ){

			// 一旦キューを削除する
#ifdef __APPLE__
#else
			mq_unlink(MSGQ_KEY);
#endif
			// デーモン指定で、自プロセスを起動する
			DBG(NULL,"daemon boot start\n");
			if( g_dbg_mode == 0 ){
				sprintf( cmd, "%s --daemon", argv[0] );
			}else{
				sprintf( cmd, "%s --daemon --DEBUG", argv[0] );
			}
			system( cmd );
			DBG(NULL,"daemon boot complete\n");
		}

		// メッセージキューをオープンする
#ifdef __APPLE__
        DBG(NULL,"[%s](%d):message que create\n", __FILE__, __LINE__);

        key_t que_key = ftok(g_prc_path, 'a');
        g_que = msgget(que_key,0666|IPC_CREAT);
        if(-1 == g_que){
            ERR(NULL,"mq_open error (%s)\n", strerror(errno));
            DBG(NULL,"[%s(%d):>>>>>>>> End program  >>>>>>>>\n", __FILE__, __LINE__);
            return(-1);
        }

        msg_buf_t msg_buf = {};
        msg_buf.mtype = 1L;
        memcpy(msg_buf.mtext, &thread_param, sizeof(thread_param));
        // メッセージキューを使用して、デーモンプロセスに起動パラメータを通知する
        DBG(NULL,"message enque\n");
        if(-1 == msgsnd(g_que, (char*)&msg_buf, MSG_BUF_SIZE, 0) ){
            ERR(NULL,"mq_send error (%s)\n", strerror(errno));
            return(-1);
        }
#else
		DBG(NULL,"[%s](%d):message que create\n", __FILE__, __LINE__);
		g_que = mq_open(MSGQ_KEY, O_RDWR|O_CREAT, 0644, NULL);
		if(-1 == g_que){
			ERR(NULL,"mq_open error (%s)\n", strerror(errno));
			DBG(NULL,"[%s(%d):>>>>>>>> End program  >>>>>>>>\n", __FILE__, __LINE__);
			return(-1);
		}
		mq_getattr(g_que, &g_mq_attr);

		// メッセージキューを使用して、デーモンプロセスに起動パラメータを通知する
		DBG(NULL,"message enque\n");
		if(-1 == mq_send(g_que, (char*)&thread_param, sizeof(thread_param), 0) ){
			ERR(NULL,"mq_send error (%s)\n", strerror(errno));
			return(-1);
		}
#endif
	}

	DBG(NULL,"[%s](%d): Initial Sequence OK\n", __FILE__, __LINE__);

	// プロセス終了
	exit(EXIT_SUCCESS);
}


//*****************************************************************************
// MODULE	:	term
// ABSTRACT	:	終了処理
// FUNCTION	:
// ARGUMENTS:	なし
// RETURN	:	なし
// NOTE		:
//*****************************************************************************
void term()
{
	char cmd[1024];

	// メッセージキューのクローズと削除
#ifdef __APPLE__
    if( g_que != -1 ){
        msgctl(g_que,IPC_RMID,NULL);
    }
#else
	if( g_que != -1 ){
		mq_close(g_que);
	}
	mq_unlink(MSGQ_KEY);
#endif
	// デーモンプロセスを停止させる
	sprintf( cmd, "pkill -9 %s &> /dev/null", g_prc_name );
	system( cmd );
	DBG(NULL, "[%s](%d):<<<<<<<< End program <<<<<<<<\n", __FILE__, __LINE__);
}

