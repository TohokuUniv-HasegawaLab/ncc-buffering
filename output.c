//*****************************************************************************
// FILENAM	:	global.c
// ABSTRAC	:	グローバル変数宣言
// NOTE		:	
//*****************************************************************************
#include	"tcpTester.h"

void calc_timer_create(thread_param_t* param_p, int kind);						// 計測タイマー生成
void calc_start(thread_param_t* param_p, int kind);								// 計測タイマー起動


//*****************************************************************************
// MODULE	:	size_string
// ABSTRACT	:	単位付き文字列変換
// FUNCTION	:	サイズを単位付き文字列に変換する
// ARGUMENTS:	ssize_t		argc	(In)  サイズ
//			:	char*		buf_p	(Out) 変換後文字列
// RETURN	:	変換後の文字列ポインタ
// NOTE		:
//*****************************************************************************
char* size_string( ssize_t size, char* buf_p )
{
	const ssize_t size_P = 1024LL * 1024LL * 1024LL * 1024LL * 1024LL;
	const ssize_t size_T = 1024LL * 1024LL * 1024LL * 1024LL;
	const ssize_t size_G = 1024LL * 1024LL * 1024LL;
	const ssize_t size_M = 1024LL * 1024LL;
	const ssize_t size_K = 1024LL;

	// byte サイズを Kbyte,Mbyte,Gbyte,Tbyte,Pbyteに変換する
	if( size_P <= (ssize_t)size ){
		sprintf( buf_p, "%.1lf Pbyte", (double)size/size_P );
	}else if( size_T <= (ssize_t)size ){
		sprintf( buf_p, "%.1lf Tbyte", (double)size/size_T );
	}else if( size_G <= (ssize_t)size ){
		sprintf( buf_p, "%.1lf Gbyte", (double)size/size_G );
	}else if( size_M <= (ssize_t)size ){
		sprintf( buf_p, "%.1lf Mbyte", (double)size/size_M );
	}else if( size_K <= (ssize_t)size ){
		sprintf( buf_p, "%.1lf Kbyte", (double)size/size_K );
	}else{
		sprintf( buf_p, "%ld byte", size );
	}

	return buf_p;
}



//*****************************************************************************
// MODULE	:	output_log
// ABSTRACT	:	ログ出力処理
// FUNCTION	:	指定されたファイル名に文字列を出力する
// ARGUMENTS:	char*	argc	(In)	ファイル名
//			:	char*	text	(In)	出力文字列
// RETURN	:	変換後の文字列ポインタ
// NOTE		:
//*****************************************************************************
void output_log(char* filename, char* text)
{
	struct	timeval	now;														// 現在時刻一時格納用変数(通算秒)
	struct	tm		now_local_time;												// 現在時刻一時格納用変数(ローカル時刻)
	FILE*			fp = NULL;													// 結果出力ファイルポインタ

	// 現在時刻取得
	gettimeofday(&now, NULL);
	memcpy( &now_local_time, localtime(&now.tv_sec), sizeof(now_local_time) );

	// 結果出力ファイルオープン
	fp = fopen( filename, "a+" );
	if( fp != (FILE*)NULL ){
		// 結果出力ファイルの書き込み
		fprintf(fp,
			"%d/%2d/%02d %02d:%02d:%02d.%06ld: %s",
			now_local_time.tm_year-100,
			now_local_time.tm_mon+1,
			now_local_time.tm_mday,
			now_local_time.tm_hour,
			now_local_time.tm_min,
			now_local_time.tm_sec,
#ifdef __APPLE__
			(long)now.tv_usec,
#else
			now.tv_usec,
#endif
			text
			);
		// 結果出力ファイルのクローズ
		fclose(fp);
	}
}


//*****************************************************************************
// MODULE	:	output_calc_result
// ABSTRACT	:	結果出力ファイル送受信結果出力
// FUNCTION	:	結果出力ファイルに送受信結果を出力する
// ARGUMENTS:	char*	argc	(In)	ファイル名
//			:	thread_param_t*	param_p			(In)	スレッド情報ポインタ
//			:	long long		first_us		(In)	初回送信時刻(us)
//			:	long long		last_us			(In)	最終送信ｍ時刻(us)
//			:	long long		now_us			(In)	現在時刻(us)
//			:	ssize_t			interval_size	(In)	送受信サイズ(1周期)
//			:	ssize_t			total_size		(In)	全送受信サイズ
//			;	int				counter			(In)	計測回数(=周期回数)
//			:	ssize_t			rate			(In)	送受信レート
//			:	char*			text			(In)	"send"/"recv"
//			:	char*	text	(In)	出力文字列
// RETURN	:	なし
// NOTE		:
//*****************************************************************************
void output_calc_result(thread_param_t* param_p, long long first_us, long long last_us, long long now_us, ssize_t interval_size, ssize_t total_size, int counter, ssize_t rate, char* text )
{
	char			text_buf[512];												// 結果出力内容一時格納変数
	char			buf_size[256];												// 単位変換後文字列一時格納変数
	long long		diff_time;													// 前回時刻との差分時刻

	// 最後に出力した時刻との差分を算出する(=周期時間)
	diff_time = now_us - last_us;

	// 差分時間が 1ms未満の場合、単位をusで表示する
	if( diff_time < 1000 ){
		// 出力結果を文字列編集する
		sprintf( text_buf,
			//"[%7.3lf - %7.3lf (%lld us)] %s \t%s(%ld byte)\t%ld byte/s\n", 
				"%7.6lf - %7.6lf %s %s %ld byte %ld byte/s\n", 
				(last_us - first_us) / 1000000.0,
				(now_us - first_us) / 1000000.0,
			//diff_time,
				text,
				size_string( interval_size, buf_size ),
				interval_size,
				rate );

	// 差分時間が 1ms異常の場合、単位をmsで表示する
	}else{
		// 出力結果を文字列編集する
		sprintf( text_buf,
			//	"[%7.3lf-%7.3lf] %s \t%s(%ld byte)\t%ld byte/s\n", 
				"%7.6lf - %7.6lf %s %s %ld byte %ld byte/s\n", 
				(last_us - first_us) / 1000000.0,
				(now_us - first_us) / 1000000.0,
				text,
				size_string( interval_size, buf_size ),
				interval_size,
				rate );
	}

	// 結果出力ファイルに出力する
	LOG( param_p, "%s", text_buf );
}


//*****************************************************************************
// MODULE	:	send_thread_output
// ABSTRACT	:	結果出力スレッド(送信用)
// FUNCTION	:	計測結果をログに出力する(送信用)
// ARGUMENTS:	void*	p	(In)	スレッド情報
// RETURN	:	NULL固定
// NOTE		:
//*****************************************************************************
void* send_thread_output(void *p)
{
	thread_param_t*	param_p = p;												// スレッド情報一時退避領域
	struct	timeval		s_last_time = {};										// 最終送信時刻
	struct	timeval		s_now_time;												// 現在時刻(通算秒)
	long long			s_first_us = 0;											// 初回送信時刻
	long long			s_last_us = 0;											// 最終送信時刻
	int					s_interval_count = 0;									// 周期カウンター
	ssize_t				s_interval_size = 0;									// 周期単位の送信サイズ
	long long			s_now_us = 0;											// 現在時刻(通算マイクロ秒)
	ssize_t				send_total_size = 0;									// 送信トータルサイズ
	long long			diff_time = 0;											// 差分時刻
	ssize_t				rate;													// 送信レート
	int					max_fd;													// 最大ファイルディスクプリタ
    fd_set				read_fds;												// タイマーファイルディスクプリタ
    fd_set				fds;													// 一時格納用タイマーファイルディスクプリタ
    ssize_t				rtn;													// 一時格納用関数戻り値
    uint64_t			exp;													// タイムアウト結果格納領域

	DBG(param_p, "%s start\n", __func__ );

	// 計測タイマー生成
	calc_timer_create( param_p, 0 );
	param_p->is_run_calc_s = STS_CALC_WAIT;
    FD_ZERO( &read_fds );
    FD_SET( param_p->s_timer_fd, &read_fds );
    FD_SET( param_p->s_stop_fd, &read_fds );
	if( param_p->s_stop_fd < param_p->s_timer_fd ){
		max_fd = param_p->s_timer_fd;
	}else{
		max_fd = param_p->s_stop_fd;
	}

	// 送信計測タイマー開始待ち
	DBG(param_p,"Waiting for measurement start(send) fd=%d, stop_fd=%d\n", param_p->s_timer_fd, param_p->s_stop_fd);
	while( 1 ){
		// 送信計測処理を開始する(初回送信のタイミングから計測を開始する)
		if( !((param_p->is_run_calc_s == STS_CALC_WAIT) && (param_p->error_flg == 0)) ){
			// 送信計測タイマー開始
			calc_start( param_p, 0 );
			gettimeofday(&s_last_time, NULL);
			s_first_us = s_last_time.tv_sec * 1000000 + s_last_time.tv_usec;
			param_p->is_run_calc_s = STS_CALC_MEASURING;
			break;
		}
		usleep(0);
	}

	DBG(param_p,"measurement start(send)\n");

	// 計測処理
	while( (param_p->is_run_calc_s == STS_CALC_MEASURING) && (param_p->error_flg == 0) ){

		// タイムアウト(周期単位でタイムアウトする)
		memcpy( &fds, &read_fds, sizeof(fd_set) );
		rtn = select( max_fd+1, &fds, NULL, NULL, NULL );
		if ( rtn > 0 ){

			// 周期タイマーのタイムアウトの場合
			if ( FD_ISSET( param_p->s_timer_fd, &fds )){
				// タイムアウト値を受け取る(readでタイムアウト検出となる)
				read( param_p->s_timer_fd, &exp, sizeof(uint64_t));

				// 現在時刻取得
				gettimeofday(&s_now_time, NULL);
				s_last_us = s_last_time.tv_sec * 1000000 + s_last_time.tv_usec;
				s_now_us = s_now_time.tv_sec * 1000000 + s_now_time.tv_usec;

				// 送信レートを算出
				s_interval_size = param_p->send_total - send_total_size;
				send_total_size += s_interval_size;
				if( param_p->interval == 1 ){
					rate = s_interval_size;
				}else{
					rate = s_interval_size / param_p->interval;
				}

				// 結果出力ファイルに書き込む
				output_calc_result(param_p, s_first_us, s_last_us, s_now_us, s_interval_size, param_p->send_total, s_interval_count, rate, "send" );
				s_interval_count = 0;
				s_last_time = s_now_time;
				s_interval_count ++;
			}

			// 計測終了の場合
			if ( FD_ISSET( param_p->s_stop_fd, &fds )){

				// 計測終了通知を受け取る(readで終了通知受信となる)
				read( param_p->s_stop_fd, &exp, sizeof(uint64_t));
				s_interval_size = param_p->send_total - send_total_size;
				if( s_interval_size > 0 ){

					// 現在時刻取得
					gettimeofday(&s_now_time, NULL);

					// 送信レートを算出
					send_total_size += s_interval_size;
					s_last_us = s_last_time.tv_sec * 1000000 + s_last_time.tv_usec;
					s_now_us = s_now_time.tv_sec * 1000000 + s_now_time.tv_usec;
					diff_time = s_now_us - s_last_us;
					rate = -1;
					if( diff_time != 0 ){
						rate = s_interval_size * 1000000 / diff_time;
					}

					// 結果出力ファイルに書き込む
					output_calc_result(param_p, s_first_us, s_last_us, s_now_us, s_interval_size, param_p->send_total, s_interval_count, rate, "send" );
					s_interval_count = 0;
					s_last_time = s_now_time;
				}
				s_interval_count ++;

				// 計測タイマーのクローズ
				close( param_p->s_timer_fd );
				param_p->s_timer_fd = 0;
			}
        }else{
			break;
		}
    }
	DBG(param_p,"measurement end(send)\n");

	DBG(param_p, "send_total_size=>%ld\n", send_total_size );

	if( param_p->mode != MODE_SEND ){
		LOG( param_p, "Finished\n" );
	}

	// 計測タイマー(送信用)のクローズ
    if( param_p->s_timer_fd != 0 ){
		close(param_p->s_timer_fd);
	}
#ifdef __APPLE__
    if( param_p->s_timer_fd_w != 0 ){
        close(param_p->s_timer_fd_w);
    }
#endif

	// 計測タイマー(送信停止用)のクローズ
     if( param_p->s_stop_fd != 0 ){
		close(param_p->s_stop_fd);
	}
#ifdef __APPLE__
    if( param_p->s_stop_fd_w != 0 ){
        close(param_p->s_stop_fd_w);
    }
#endif
    
	DBG(param_p, "%s end\n", __func__ );
	return NULL;
}


//*****************************************************************************
// MODULE	:	recv_thread_output
// ABSTRACT	:	結果出力スレッド(受信用)
// FUNCTION	:	計測結果をログに出力する(受信用)
// ARGUMENTS:	void*	p	(In)	スレッド情報
// RETURN	:	NULL固定
// NOTE		:
//*****************************************************************************
void* recv_thread_output(void *p)
{
	thread_param_t*	param_p = p;												// スレッド情報一時退避領域
	struct	timeval		r_last_time = {};										// 最終送信時刻
	struct	timeval		r_now_time;												// 現在時刻(通算秒)
	long long			r_first_us = 0;											// 初回送信時刻
	long long			r_last_us = 0;											// 最終送信時刻
	int					r_interval_count = 0;									// 周期カウンター
	ssize_t				r_interval_size = 0;									// 周期単位の送信サイズ
	long long			r_now_us = 0;											// 現在時刻(通算マイクロ秒)
	ssize_t				recv_total_size = 0;									// 受信トータルサイズ
	long long			diff_time = 0;											// 差分時刻
	ssize_t				rate;													// 送信レート
	int					max_fd;													// 最大ファイルディスクプリタ
    fd_set				read_fds;												// タイマーファイルディスクプリタ
    fd_set				fds;													// 一時格納用タイマーファイルディスクプリタ
    ssize_t				rtn;													// 一時格納用関数戻り値
    uint64_t			exp;													// タイムアウト結果格納領域


	DBG(param_p, "%s start\n", __func__ );

	// 計測タイマー生成
	calc_timer_create( param_p, 1 );
	param_p->is_run_calc_r = STS_CALC_WAIT;
    FD_ZERO( &read_fds );
    FD_SET( param_p->r_timer_fd, &read_fds );
    FD_SET( param_p->r_stop_fd, &read_fds );
	if( param_p->r_stop_fd < param_p->r_timer_fd ){
		max_fd = param_p->r_timer_fd;
	}else{
		max_fd = param_p->r_stop_fd;
	}

	// 受信計測タイマー開始待ち
	DBG(param_p,"Waiting for measurement start(recv) fd=%d, stop_fd=%d\n", param_p->r_timer_fd, param_p->r_stop_fd);
	while( 1 ){

		// 受信計測処理を開始する(初回送信のタイミングから計測を開始する)
		if( !((param_p->is_run_calc_r == STS_CALC_WAIT) && (param_p->error_flg == 0)) ){

			// 受信計測タイマー開始
			calc_start( param_p, 1 );
			gettimeofday(&r_last_time, NULL);
			r_first_us = r_last_time.tv_sec * 1000000 + r_last_time.tv_usec;
			param_p->is_run_calc_r = STS_CALC_MEASURING;
			break;
		}
		usleep(0);
	}
	DBG(param_p,"measurement start(recv)\n");

	// 計測処理
	while( (param_p->is_run_calc_r == STS_CALC_MEASURING) && (param_p->error_flg == 0)){

		// タイムアウト(周期単位でタイムアウトする)
		memcpy( &fds, &read_fds, sizeof(fd_set) );
		rtn = select( max_fd+1, &fds, NULL, NULL, NULL );
		if ( rtn > 0 ){

			// 周期タイマーのタイムアウトの場合
			if ( FD_ISSET( param_p->r_timer_fd, &fds )){
				// タイムアウト値を受け取る(readでタイムアウト検出となる)
				read( param_p->r_timer_fd, &exp, sizeof(uint64_t));

				// 現在時刻取得
				gettimeofday(&r_now_time, NULL);
				r_last_us = r_last_time.tv_sec * 1000000 + r_last_time.tv_usec;
				r_now_us = r_now_time.tv_sec * 1000000 + r_now_time.tv_usec;

				// 受信レートを算出
				r_interval_size = param_p->recv_total - recv_total_size;
				recv_total_size += r_interval_size;
				if( param_p->interval == 1 ){
					rate = r_interval_size;
				}else{
					rate = r_interval_size / param_p->interval;
				}

				// 結果出力ファイルに書き込む
				output_calc_result(param_p, r_first_us, r_last_us, r_now_us, r_interval_size, param_p->recv_total, r_interval_count, rate, "recv" );
				r_interval_count = 0;
				r_last_time = r_now_time;
				r_interval_count ++;
			}

			// 計測終了の場合
			if ( FD_ISSET( param_p->r_stop_fd, &fds )){

				// 計測終了通知を受け取る(readで終了通知受信となる)
				read( param_p->r_stop_fd, &exp, sizeof(uint64_t));
				r_interval_size = param_p->recv_total - recv_total_size;
				if( r_interval_size > 0 ){

					// 現在時刻取得
					gettimeofday(&r_now_time, NULL);

					// 受信レートを算出
					recv_total_size += r_interval_size;
					r_last_us = r_last_time.tv_sec * 1000000 + r_last_time.tv_usec;
					//r_now_us = param_p->o_buf[param_p->r_pos].now_time.tv_sec * 1000000 + param_p->o_buf[param_p->r_pos].now_time.tv_usec;
					r_now_us = r_now_time.tv_sec * 1000000 + r_now_time.tv_usec;
					diff_time = r_now_us - r_last_us;
					rate = -1;
					if( diff_time != 0 ){
						rate = r_interval_size * 1000000 / diff_time;
					}

					// 結果出力ファイルに書き込む
					output_calc_result(param_p, r_first_us, r_last_us, r_now_us, r_interval_size, param_p->recv_total, r_interval_count, rate, "recv" );
					r_interval_count = 0;
					r_last_time = r_now_time;
				}
				r_interval_count ++;

				// 計測タイマーのクローズ
				close( param_p->r_timer_fd );
				param_p->r_timer_fd = 0;
			}
        }else{
			break;
		}
    }
	DBG(param_p,"measurement end(recv)\n");
	DBG(param_p, "recv_total_size=>%ld\n", recv_total_size );
	if( param_p->mode == MODE_SEND ){
		LOG( param_p, "Finished\n" );
	}

	// 計測タイマー(送信用)のクローズ
    if( param_p->r_timer_fd != 0 ){
		close(param_p->r_timer_fd);
	}
#ifdef __APPLE__
    if( param_p->r_timer_fd_w != 0 ){
        close(param_p->r_timer_fd_w);
    }
#endif
	// 計測タイマー(送信用)のクローズ
    if( param_p->r_stop_fd != 0 ){
		close(param_p->r_stop_fd);
	}
#ifdef __APPLE__
    if( param_p->r_stop_fd != 0 ){
        close(param_p->r_stop_fd_w);
    }
#endif
	DBG(param_p, "%s end\n", __func__ );
	return NULL;
}


//*****************************************************************************
// MODULE	:	calc_timer_create
// ABSTRACT	:	計測タイマー生成
// FUNCTION	:	計測タイマーを生成する
// ARGUMENTS:	thread_param_t*	param_p	(In)	スレッド情報
//			:	int				kind	(In)	種別(0=送信計測,1=受信計測)
// RETURN	:	NULL固定
// NOTE		:
//*****************************************************************************
void calc_timer_create(thread_param_t* param_p, int kind)
{
#ifdef __APPLE__
	// MacOS用タイマーの作成
    dispatch_queue_t queue = dispatch_queue_create("timerQueue", 0);

    int timer_fd[2] = {};
    int stop_fd[2] = {};
	
    // パイプを作成し、タイマー起動時にパイプへ書き込みを行うことにより、timerfd_createと同様の動作をさせる
    pipe(timer_fd);
    pipe(stop_fd);

    if( kind == 0 ){
        param_p->s_timer_t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
        param_p->s_stop_t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
        
        param_p->s_timer_fd = timer_fd[0];
        param_p->s_stop_fd = stop_fd[0];
        param_p->s_timer_fd_w = timer_fd[1];
        param_p->s_stop_fd_w = stop_fd[1];
    }else{
        param_p->r_timer_t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
        param_p->r_stop_t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
        
        param_p->r_timer_fd = timer_fd[0];
        param_p->r_stop_fd = stop_fd[0];
        param_p->r_timer_fd_w = timer_fd[1];
        param_p->r_stop_fd_w = stop_fd[1];
    }

#else
	if( kind == 0 ){
	    param_p->s_timer_fd = timerfd_create( CLOCK_MONOTONIC, 0 );
	    param_p->s_stop_fd = timerfd_create( CLOCK_MONOTONIC, 0 );
	}else{
	    param_p->r_timer_fd = timerfd_create( CLOCK_MONOTONIC, 0 );
    	param_p->r_stop_fd = timerfd_create( CLOCK_MONOTONIC, 0 );
	}
#endif
}


//*****************************************************************************
// MODULE	:	calc_start
// ABSTRACT	:	計測タイマー起動(=計測開始)
// FUNCTION	:	計測タイマーを起動する
// ARGUMENTS:	thread_param_t*	param_p	(In)	スレッド情報
//			:	int				kind	(In)	種別(0=送信計測,1=受信計測)
// RETURN	:	NULL固定
// NOTE		:
//*****************************************************************************
void calc_start(thread_param_t* param_p, int kind)
{
#ifdef __APPLE__

    if( kind == 0 ){
        if( param_p->s_timer_fd != 0 ){

            __block int fb = param_p->s_timer_fd_w;
            dispatch_source_set_timer(param_p->s_timer_t, dispatch_time(DISPATCH_TIME_NOW, 0), param_p->interval * NSEC_PER_SEC, 0);
            dispatch_source_set_event_handler(param_p->s_timer_t, ^{
            	// 書き込み用パイプへの書き込み
                write(fb,"¥0",1);
            });
            dispatch_resume(param_p->s_timer_t);
        }
    }else{
        if( param_p->r_timer_fd != 0 ){
            __block int fb = param_p->r_timer_fd_w;
            dispatch_source_set_timer(param_p->r_timer_t, dispatch_time(DISPATCH_TIME_NOW, 0), param_p->interval * NSEC_PER_SEC, 0);
            dispatch_source_set_event_handler(param_p->r_timer_t, ^{
            	// 書き込み用パイプへの書き込み
                write(fb,"¥0",1);
            });
            dispatch_resume(param_p->r_timer_t);
        }
    }
#else
    struct itimerspec start;

    start.it_value.tv_sec  = param_p->interval;
    start.it_value.tv_nsec = 0;
    start.it_interval.tv_sec  = start.it_value.tv_sec;
    start.it_interval.tv_nsec = start.it_value.tv_nsec;
	if( kind == 0 ){
		if( param_p->s_stop_fd != 0 ){
			timerfd_settime( param_p->s_timer_fd, 0, &start, NULL );
		}
	}else{
		if( param_p->r_stop_fd != 0 ){
			timerfd_settime( param_p->r_timer_fd, 0, &start, NULL );
		}
	}
#endif
}


//*****************************************************************************
// MODULE	:	calc_stop
// ABSTRACT	:	計測タイマー停止(=計測停止)
// FUNCTION	:	計測タイマーを停止する
// ARGUMENTS:	thread_param_t*	param_p	(In)	スレッド情報
//			:	int				kind	(In)	種別(0=送信計測,1=受信計測)
// RETURN	:	NULL固定
// NOTE		:
//*****************************************************************************
void calc_stop(thread_param_t* param_p, int kind)
{
#ifdef __APPLE__

    if( kind == 0 ){
        if( param_p->s_stop_fd != 0 ){
            __block int fb = param_p->s_stop_fd_w;
            dispatch_source_set_timer(param_p->s_stop_t, dispatch_time(DISPATCH_TIME_NOW, 0), 1, 0);
            dispatch_source_set_event_handler(param_p->s_stop_t, ^{
            	// 書き込み用パイプへの書き込み
                write(fb,"¥0",1);
            });
            dispatch_resume(param_p->s_stop_t);
        }
    }else{
        if( param_p->r_stop_fd != 0 ){
            __block int fb = param_p->r_stop_fd_w;
            dispatch_source_set_timer(param_p->r_stop_t, dispatch_time(DISPATCH_TIME_NOW, 0), 1, 0);
            dispatch_source_set_event_handler(param_p->r_stop_t, ^{
            	// 書き込み用パイプへの書き込み
                write(fb,"¥0",1);
            });
            dispatch_resume(param_p->r_stop_t);

        }
    }
#else
    struct itimerspec stop={};

    stop.it_value.tv_sec  = 0;
    stop.it_value.tv_nsec = 1;
    stop.it_interval.tv_sec  = 0;
    stop.it_interval.tv_nsec = 1;
	if( kind == 0 ){
		if( param_p->s_stop_fd != 0 ){
			timerfd_settime( param_p->s_stop_fd, 0, &stop, NULL );
		}
	}else{
		if( param_p->r_stop_fd != 0 ){
			timerfd_settime( param_p->r_stop_fd, 0, &stop, NULL );
		}
	}
#endif
}
