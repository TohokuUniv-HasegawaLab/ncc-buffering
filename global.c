//*****************************************************************************
// FILENAM	:	global.c
// ABSTRAC	:	グローバル変数宣言
// NOTE		:	
//*****************************************************************************
#include	"tcpTester.h"

char			g_prc_name[256] = {};											// 自プロセス名
char			g_prc_path[256] = {};											// カレントパス
char			g_result_file[256+32] = { "/tmp/result.txt" };					// 結果出力ファイル名(デフォルト用)
int				g_dbg_mode = 0;													// デバッグモード(0=OFF, 1=ON)
int				g_dbg_mode2 = 0;												// デバッグモード2(0=OFF, 1=ON)
int				g_thread_no = 0;												// スレッド番号
thread_mng_t	g_thread_mngtbl[THREAD_MAX];									// スレッド管理情報
#ifdef __APPLE__
int				g_que;
#else
mqd_t			g_que;															// メッセージキュー記述子
struct mq_attr	g_mq_attr;														// キュー属性情報
#endif



//*****************************************************************************
// MODULE	:	deg_dsp_variable
// ABSTRACT	:	内部変数内容表示
// FUNCTION	:
// ARGUMENTS:	void *thread_param		(In) スレッドパラメータのポインタ
// RETURN	:	
//			:	
// NOTE		:
//*****************************************************************************
void deg_dsp_variable(thread_param_t* param_p)
{
	int	ii;																		// 汎用カウンタ

	// 内部変数の内容をデバッグ出力する
	DBG(param_p,"---- variable disp start ----\n");
	DBG(param_p,"g_prc_name           = %s\n", g_prc_name  );
	DBG(param_p,"g_prc_path           = %s\n", g_prc_path  );
	DBG(param_p,"g_dbg_mode           = %d\n", g_dbg_mode  );
	DBG(param_p,"g_result_file        = %s\n", g_result_file  );
#ifndef __APPLE__
	DBG(param_p,"g_mq_attr.mq_msgsize = %ld\n", g_mq_attr.mq_msgsize );
#endif
	DBG(param_p,"thread_param.thread_no       = %d\n",	param_p->thread_no		);
	DBG(param_p,"thread_param.mode            = %d\n",	param_p->mode			);
	DBG(param_p,"thread_param.ip              = %s\n",	param_p->ip				);
	DBG(param_p,"thread_param.port            = %d\n",	param_p->port			);
	DBG(param_p,"thread_param.size            = %ld\n",	param_p->size			);
	DBG(param_p,"thread_param.congestion_mode = %d\n",	param_p->congestion_mode);
	DBG(param_p,"thread_param.congestion_name = %s\n",	param_p->congestion_name);
	DBG(param_p,"thread_param.app_mode        = %d\n",	param_p->app_mode		);
	DBG(param_p,"thread_param.app_filename    = %s\n",	param_p->app_filename	);
	DBG(param_p,"thread_param.input_filename  = %s\n",	param_p->input_filename	);
	DBG(param_p,"thread_param.output_filename = %s\n",	param_p->output_filename);
	DBG(param_p,"thread_param.send_start_time = %ld\n",	param_p->send_start_time);
	DBG(param_p,"thread_param.buf_size        = %ld\n",	param_p->buf_size		);
	DBG(param_p,"thread_param.result_filename = %s\n",	param_p->result_filename);
	DBG(param_p,"thread_param.save_filename   = %s\n",	param_p->save_filename	);
	DBG(param_p,"thread_param.auto_port       = %d\n",	param_p->auto_port		);
	DBG(param_p,"thread_param.ring_bufsize    = %ld\n",	param_p->ring_bufsize	);
	DBG(param_p,"thread_param.snd_bufsizse    = %d\n",	param_p->snd_bufsize	);
	DBG(param_p,"thread_param.rcv_bufsizse    = %d\n",	param_p->rcv_bufsize	);
	DBG(param_p,"thread_param.timeout         = %d\n",	param_p->timeout		);
	DBG(param_p,"thread_param.dbg_mode        = %d\n",	param_p->dbg_mode		);
	DBG(param_p,"thread_param.dbg_mode2       = %d\n",	param_p->dbg_mode2		);
	DBG(param_p,"g_thread_no = %d\n", g_thread_no );

	// スレッド管理テーブルの内容をデバッグ出力する
	for( ii=0; ii<THREAD_MAX; ii++ ){
		if( g_thread_mngtbl[ii].used_flg == 1 ){
			DBG(param_p,"g_thread_mngtbl[%5d].used_flg             = %d\n",	ii, g_thread_mngtbl[ii].used_flg					);
			DBG(param_p,"g_thread_mngtbl[%5d].param.thread_no      = %d\n",	ii, g_thread_mngtbl[ii].param.thread_no				);
			DBG(param_p,"g_thread_mngtbl[%5d].param.mode           = %d\n",	ii, g_thread_mngtbl[ii].param.mode					);
			DBG(param_p,"g_thread_mngtbl[%5d].param.ip             = %s\n",	ii, g_thread_mngtbl[ii].param.ip					);
			DBG(param_p,"g_thread_mngtbl[%5d].param.port           = %d\n",	ii, g_thread_mngtbl[ii].param.port					);
			DBG(param_p,"g_thread_mngtbl[%5d].param.size           = %ld\n",ii, g_thread_mngtbl[ii].param.size					);
			DBG(param_p,"g_thread_mngtbl[%5d].param.congestion_mode= %d\n",	ii, g_thread_mngtbl[ii].param.congestion_mode		);
			DBG(param_p,"g_thread_mngtbl[%5d].param.congestion_name= %s\n",	ii, g_thread_mngtbl[ii].param.congestion_name		);
			DBG(param_p,"g_thread_mngtbl[%5d].param.app_mode       = %d\n",	ii, g_thread_mngtbl[ii].param.app_mode				);
			DBG(param_p,"g_thread_mngtbl[%5d].param.app_filename   = %s\n",	ii, g_thread_mngtbl[ii].param.app_filename			);
			DBG(param_p,"g_thread_mngtbl[%5d].param.input_filename = %s\n",	ii, g_thread_mngtbl[ii].param.input_filename		);
			DBG(param_p,"g_thread_mngtbl[%5d].param.output_filename= %s\n",	ii, g_thread_mngtbl[ii].param.output_filename		);
			DBG(param_p,"g_thread_mngtbl[%5d].param.send_start_time= %ld\n",ii, g_thread_mngtbl[ii].param.send_start_time		);
			DBG(param_p,"g_thread_mngtbl[%5d].param.buf_size       = %ld\n",ii, g_thread_mngtbl[ii].param.buf_size				);
			DBG(param_p,"g_thread_mngtbl[%5d].result_filename      = %s\n",	ii, g_thread_mngtbl[ii].param.result_filename		);
			DBG(param_p,"g_thread_mngtbl[%5d].save_filename        = %s\n",	ii, g_thread_mngtbl[ii].param.save_filename			);
			DBG(param_p,"g_thread_mngtbl[%5d].param.auto_port      = %d\n",	ii, g_thread_mngtbl[ii].param.auto_port				);
			DBG(param_p,"g_thread_mngtbl[%5d].param.ring_bufsize   = %ld\n",ii, g_thread_mngtbl[ii].param.ring_bufsize			);
			DBG(param_p,"g_thread_mngtbl[%5d].param.snd_bufsize    = %d\n",	ii, g_thread_mngtbl[ii].param.snd_bufsize			);
			DBG(param_p,"g_thread_mngtbl[%5d].param.rcv_bufsize    = %d\n",	ii, g_thread_mngtbl[ii].param.rcv_bufsize			);
			DBG(param_p,"g_thread_mngtbl[%5d].param.timeout        = %d\n",	ii, g_thread_mngtbl[ii].param.timeout				);
			DBG(param_p,"g_thread_mngtbl[%5d].param.dbg_mode       = %d\n",	ii, g_thread_mngtbl[ii].param.dbg_mode				);
			DBG(param_p,"g_thread_mngtbl[%5d].param.dbg_mode2      = %d\n",	ii, g_thread_mngtbl[ii].param.dbg_mode2				);
		}
	}
	DBG(param_p,"---- variable disp end ----\n");
}
