//*****************************************************************************
// FILENAM	:	help.c
// ABSTRAC	:	ヘルプ表示処理
// NOTE		:	
//*****************************************************************************
#include	"tcpTester.h"


//*****************************************************************************
// MODULE	:	disp_help
// ABSTRACT	:	ヘルプ表示
// FUNCTION	:
// ARGUMENTS:	INT    argc   (In) 引数の数
//			:	CHAR** argv   (In) 引数のポインタリスト
// RETURN	:	 0 = 正常終了
//			:	-1 = エラー発生
// NOTE		:
//*****************************************************************************
void disp_help(int argc, char** argv)
{
#ifndef __APPLE__
	argc=argc;
#endif
	// Usageの内容を画面出力する
	DSP("\n");
	DSP("Usage: %s [-s host | -r host | -d ] [option]\n", g_prc_name );
	DSP("       %s [-h | --help ]\n", g_prc_name );
	DSP("\n");
	DSP("Source or Relay or Destination\n" );
	DSP("  -f, --test_filename  <filename>             test file name (default=/tmp/result.txt)\n");
	DSP("  -b, --bufsize        <nnnn>                 buffer size (default=128K byte: more than 1)\n");
	DSP("  -i, --interval       <nn>                   reports interval(default=1s:1-60)\n" );
	DSP("  -p, --port           <nnnn>                 server port to listen on/connect to (defalut=auto select:20000～29999)\n");
    DSP("  -T, --timeout        <nn>                   timeout sec(default=30s:more than 1\n");
    DSP("  -h, --help                                  show this message and quit\n");
    DSP("      --daemon                                start as daemon\n");
	DSP("  -Q, --quit                                  forced termination\n");
	DSP("\n");
	DSP("Source specific:\n");
    DSP("  -s, --source         <host>                 run in send node\n");
	DSP("  -l, --length         <nnnn>                 data length (default=2Gbyte: more than 1)\n");
	DSP("  -t, --starttime      <HH:MM:SS>             time to start sending\n");
    DSP("  -C, --congestion     <algo>                 set TCP congestion control algorithm(algo=cubic, reno, ccp)\n");
	DSP("  -S, --SND_BUFSIZE    <nnnn>                 send buffer size(default=/proc/sys/net/ipv4/tcp_wmem [max buffersize]\n");
    DSP("    , --so             <host>                 outbound send node\n");
	DSP("\n");
	DSP("Relay specific:\n");
    DSP("  -r, --relay          <host>                 run in relay node\n");
    DSP("  -C, --congestion     <algo>                 set TCP congestion control algorithm(algo=cubic, reno, ccp)\n");
    DSP("  -A, --app_mode       <aplication filename>  aplication file name (default=app_sample.sh)\n");
    DSP("  -I, --i_datname      <input filename>       aplication input file name(default=input.dat)\n");
    DSP("  -O, --o_datname      <output filename>      aplication output file name(default=output.dat)\n");
	DSP("  -B, --buffer_size    <nnnn>                 transport buffer size(default=250M: more than 1)\n");
	DSP("  -S, --SND_BUFSIZE    <nnnn>                 send buffer size(default=/proc/sys/net/ipv4/tcp_wmem [max buffersize]\n");
	DSP("  -R, --RCV_BUFSIZE    <nnnn>                 recv buffer size(default=/proc/sys/net/ipv4/tcp_rmem [max buffersize]\n");
    DSP("    , --ro             <host>                 outbound relay node\n");
	DSP("\n");
	DSP("Destination specific:\n");
    DSP("  -d, --server                                run in recv node\n");
    DSP("  -W, --save_filename  <save_filename>        file name for receive data\n");
	DSP("  -R, --RCV_BUFSIZE    <nnnn>                 recv buffer size(default=/proc/sys/net/ipv4/tcp_rmem [max buffersize]\n");
    DSP("    , --do             <host>                 outbound recv node\n");
	DSP("\n");
}

