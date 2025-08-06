#!/bin/bash
#
# Ver 1.0.2
#

argv=("$@")																		# 本シェルの引数
#PASSWORD=NI7474TE@ad															# パスワード
APL="~/tcpTester"																# アプリ格納位置
NOW_TS=`date "+%Y%m%d_%H%M%S"`													# 現在時刻
TEST_NAME=`printf "T%05d" $RANDOM`												# 実験名
RESULT_FILE="/tmp/${TEST_NAME}_${NOW_TS}.txt"									# 結果出力ファイル名
RESULT_FILE_DEFAULT="/tmp/${TEST_NAME}_${NOW_TS}.txt"							# 結果出力ファイル名のデフォルト名 #"/tmp/${TEST_NAME}_${NOW_TS}.txt"
SSH_COMMAND="ssh"																# ssh コマンドのデフォルト
MIN_PORT_NO=20000																# ポート番号最小値
MAX_PORT_NO=29999																# ポート番号最大値
MIN_BUFSIZE=1																	# 最小バッファサイズ(byte)
MAX_BUFSIZE=1048576																# 最大バッファサイズ(byte)
MIN_LENGTH=1																	# 最小送信データ長(byte)
MIN_RNG_BUFSIZE=262144															# 最小リングバッファサイズ(byte)
MAX_RNG_BUFSIZE=536870912														# 最大リングバッファサイズ(byte)
MIN_INTERVAL=1																	# 最小計測周期(s)
MAX_INTERVAL=60																	# 最大計測周期(s)
MIN_SNDBUFSIZE=4096																# カーネルの最小送信バッファサイズ
MAX_SNDBUFSIZE=4194304															# カーネルの最大送信バッファサイズ
MIN_RCVBUFSIZE=4096																# カーネルの最小受信バッファサイズ
MAX_RCVBUFSIZE=5666432															# カーネルの最大受信バッファサイズ
MIN_TIMEOUT=0																	# 最小タイムアウト時間
MAX_TIMEOUT=1800																# 最大タイムアウト時間
SSH_PORT=22																		# sshポート番号
quit_mode=0																		# 強制終了フラグ(-Q オプションを指定したかどうか)
notice_port=30000																# 受信完了通知ポート番号

#------------------------------------------------------------------------------
# help 表示
#------------------------------------------------------------------------------
function disp_help()
{
	echo "Usage: ttManager.sh [-s host | -r host | -d] [option]"
	echo "       ttManager.sh [-h]"
	echo ""
	echo "Souce or Relay or Destination"
	echo "  -f <test_filename>       test file name (default=/tmp/TXXXXX_YYYYMDD_HHMMSS.txt)"
	echo "  -b <nnnn>                buffer size (default=128K byte)"
	echo "                             setting range=${MIN_BUFSIZE}-${MAX_BUFSIZE}"
	echo "  -E <ssh command>         ssh command"
	echo "                             ex) ttManager.sh  -E \"sshpassh -p password ssh\""
	echo "  -i <nn>                  reports interval(default=1s)"
	echo "                             setting ragen=${MIN_INTERVAL}-${MAX_INTERVAL}"
	echo "  -p <nnnn>                destination port to listen on/connect to (default=auto select)"
	echo "                             setting range=${MIN_PORT_NO}-${MAX_PORT_NO}"
	echo "  -T <nn>                  send or recv timeout sec (default=30s)"
	echo "                             setting range=${MIN_TIMEOUT}-${MAX_TIMEOUT}"
	echo "  -h                       show help message"
	echo "  -Q                       forced termination"
	echo ""
	echo "Souce specific:"
	echo "  -s <host>                run in send node"
	echo "  -l <nnnn>                data length (default=2Gbyte)"
	echo "                             setting range=more than 1"
	echo "  -t <HH:MM:SS>            time to start sending"
	echo "  -C <algo>                set TCP congestion control algorithm(algo=cubic, reno, ccp)"
	echo "  -S <nnnn>                send buffer size(default=max buffersize)"
	echo "                             setting renge=${MIN_SNDBUFSIZE}-${MAX_SNDBUFSIZE}"
	echo "                             reference) /proc/sys/net/ipv4/tcp_wmem => min buffersize-max maffersize"
	echo "  --so <host>              outbound send node"
	echo "  --DEBUG                  debuging log output"
	echo ""
	echo "Relay specific:"
	echo "  -r <host>                run in relay node"
	echo "  -C <algo>                set TCP congestion control algorithm(algo=cubic, reno, ccp)"
	echo "  -A <apl filename>        aplication file name"
	echo "  -I <apl input filename>  aplication input file name(default=input.dat)"
	echo "  -O <apl output filename> aplication output file name(default=output.dat)"
	echo "  -B <nnnn>                ring buffer size(default=250M)"
	echo "                             setting range=more than buffer_size(-b) * 2"
	echo "  -S <nnnn>                send buffer size(default=max buffersize)"
	echo "                             setting renge=${MIN_SNDBUFSIZE}-${MAX_SNDBUFSIZE}"
	echo "                             reference) /proc/sys/net/ipv4/tcp_wmem"
	echo "  -R <nnnn>                recv buffer size(default=max buffersize)"
	echo "                             setting renge=${MIN_RCVBUFSIZE}-${MAX_RCVBUFSIZE}"
	echo "                             reference) /proc/sys/net/ipv4/tcp_rmem"
	echo "  --ro <host>              outbound relay node"
	echo "  --DEBUG                  debuging log output"
	echo ""
	echo "Destination specific:"
	echo "  -d <host>                run in recv node"
	echo "  -W <save filename>       file name for receive data"
	echo "  -R <nnnn>                recv buffer size(default=max buffersize)"
	echo "                             setting renge=${MIN_RCVBUFSIZE}-${MAX_RCVBUFSIZE}"
	echo "                             reference) /proc/sys/net/ipv4/tcp_rmem"
	echo "  --do <host>              outbound recv node"
	echo "  --DEBUG                  debuging log output"
	echo ""
	
}


#------------------------------------------------------------------------------
# update option
#  実行するコマンド情報内にあるオプションの内容を
#  引数で指定されたオプションパラメータの内容に置き換え返却する
#------------------------------------------------------------------------------
update_option()
{
	key_item=$1																	# オプション[-xx]格納エリア
	key_item_list=(${key_item// / })
	cmd_item=$2																	# オプションパラメータ格納エリア
	cmd_item_list=(${cmd_item// / })
	found=0

	# 実行するコマンド情報内を検索
	for ((key=0; key<${#key_item_list[*]}; key++ )); do
		found=0
		for ((item=0; item<${#cmd_item_list[*]}; item++ )); do
			# 既に設定されている場合、引数で受け取った内容に置き換える
			if [ "${key_item_list[$key]}" == "${cmd_item_list[$item]}" ]; then
				cmd_item_list[$item+1]=${key_item_list[$key+1]}
				item=$((item+1))
				key=$((key+1))
				found=1
			fi
		done
		# 見つからなかった場合は、オプション情報を追加する
		if [ ${found} -eq 0 ]; then
			add_key="${key_item_list[$key]} ${key_item_list[$key+1]}"
			cmd_item_list=("${cmd_item_list[@]}" ${add_key} ); 
		fi
	done
	retval="${cmd_item_list[*]}"
}


#------------------------------------------------------------------------------
# auto exit tail
#   tail 実行中に、キーワード文字(Finished or ERROR)が
#   見つかったら、tail コマンドを終了させる
#   キーワード文字または、ERRORを見つけたたら tail を終了する
#------------------------------------------------------------------------------
auto_exit_tail() {
    sleep 1
    tail_pid=""
    
    while [ "${tail_pid}" = "" ]
    do
	# tail コマンドの pid を取得
	tail_pid=`ps -ef | grep "${tail_command}" | grep -v grep | awk 'NR==1 {printf $2}' `
    done
	while read i
	do
		# 内容表示
		echo "${i}"

		# 表示内容に ERROR を見つけた場合、tail を終了する
		error_text=`echo ${i} | grep "ERROR"`
		if [ -n "${error_text}" ]; then
			kill -9 "$tail_pid" > /dev/null
			wait
			break
		fi

		# 表示内容に キーワードを見つけた場合、tail を終了する
		search_text=`echo ${i} | grep "${search_word}"`
		if [ -n "${search_text}" ]; then
			kill -9 "$tail_pid" > /dev/null
			wait
			break
		fi
	done
}


#------------------------------------------------------------------------------
# start_wait
#   受信ノード(中継ノード)の起動をチェックする
#   結果出力ファイルを監視(tailコマンド)し Start port の文字列が出現するまで待つ
#   発見したら監視を終了し、Start port で見つけたポート番号を返す
#------------------------------------------------------------------------------
start_wait() {

	search_word="Start port"
    tail_pid=""

    while [ "${tail_pid}" = "" ]
    do
	# tail コマンドの pid を取得
	tail_pid=`ps -ef | grep "${tail_command}" | grep -v grep | awk 'NR==1 {printf $2}' `
    done
	while read i
	do
		# 表示内容に ERROR を見つけた場合、tail を終了しERRORを返却する
		error_text=`echo ${i} | grep "ERROR"`
		if [ -n "${error_text}" ]; then
			start_port="ERROR"
			kill -9 "$tail_pid" &> /dev/null
			wait
			break
		fi

		# 表示内容に、Start Portを見つけた場合、tail を終了しポート番号を返す
		search_text=`echo ${i} | grep "${search_word}"`
		if [ -n "${search_text}" ]; then
			start_port=`echo ${search_text} | awk '{ printf $NF }'`
			kill -9 "$tail_pid" &> /dev/null
			wait
			break
		fi
	done
	echo "${start_port}"
}


#------------------------------------------------------------------------------
# init param
#   引数のオプションから各サーバ毎のコマンドリストを生成する
#------------------------------------------------------------------------------
function init_param()
{
	back=-1
	mode=0
	sv_num=0
	change_flg=-1
	command_replace_mode=0
	quit_flg=0

	#パラメータ数分ループする
	for i in `seq 1 $#`
	do
		case "${argv[$i-1]}" in
		"-s") mode=1; change_flg=1; sv_num=$((sv_num+1));;
		"-r") mode=2; change_flg=1; sv_num=$((sv_num+1));;
		"-d") mode=3; change_flg=1; sv_num=$((sv_num+1));;
		"-h") mode=4;;
		"-E") mode=5; 
				# [-E] オプションのパラメータが設定されているかチェックする
				check_val="${argv[$i]}"
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
					echo "[-E] ssh command is empty"
					return 1
				fi
				# [-E] オプションのパラメータが実行可能かチェックする
				${check_val} &> /dev/null
				if [ $? -eq 127 ]; then
					echo "[-E] ssh command error(${check_val})"
					return 1
				fi
				continue;;
		*)	if [ ${mode} -eq 5 ]; then
				# [-E] オプションのパラメータの正当性をチェックする
				check_val="${argv[$i-1]}"
				${check_val} 127.0.0.1 ls &> /dev/null
				if [ $? -ne 0 ]; then
					echo "ssh command or password error (${check_val})"
					return 1
				fi
				SSH_COMMAND="${argv[$i-1]}"
				mode=0
				continue
			fi
		esac
		if [ ${change_flg} -eq -1 ]; then
			option=${argv[$i-1]}
			back=${mode}
		else
			if [ ${change_flg} -eq 0 ]; then
				option+=" ${argv[$i-1]}"
			else
				if [ ${back} -eq 0 ]; then
					com_inf=("${com_inf[@]}" "${option}"); 
				else
					svr_inf=("${svr_inf[@]}" "${option}"); 
				fi
				option=${argv[$i-1]}
			fi
			back=${mode}
		fi
		change_flg=0;
	done
	if [ "${option}" != "" ]; then
		svr_inf=("${svr_inf[@]}" "${option}"); 
	fi
	if [ ${mode} -eq 4 ]; then
		disp_help
		exit 0
	fi

#echo "#*********** debug ***********"
#	for ((i=0; i<(${#com_inf[*]}); i++)); do
#		echo "===> com_inf[${#com_inf[*]}]: $[i] : ${com_inf[$i]}"
#	done
#	for ((i=0; i<(${#svr_inf[*]}); i++)); do
#		echo "===> svr_inf[${#svr_inf[*]}]: $[i] : ${svr_inf[$i]}"
#	done
#echo "#*********** debug ***********"

	result_file_default=0
	if [ "${RESULT_FILE}" == "${RESULT_FILE_DEFAULT}" ]; then
		result_file_default=1
	fi
	# sourceサーバのコマンドリストからIPアドレスを取得する
	for ((i=${#svr_inf[*]}; 0<i; i--)); do
		cmd=${svr_inf[$i-1]}
		list=(${cmd// / })
		if [ "${list[0]}" == "-s" ]; then
		
			opt_so=-1
			for ((j=2; j<${#list[*]}; j++)); do
				if [ "${list[j]}" == "--so" ]; then
					opt_so=$j
				fi
			done
			
			if [ "$opt_so" -ne -1 ]; then
				next_ip="${list[opt_so+1]}"
			else
				next_ip="${list[1]}"
			fi
		fi
	done
	
	result_files[0]="${RESULT_FILE}"
	
	# destinationサーバのコマンドリストを生成する
	for ((i=${#svr_inf[*]}; 0<i; i--)); do
		cmd=${svr_inf[$i-1]}
		list=(${cmd// / })
		if [ "${list[0]}" == "-d" ]; then

			opt_do=-1
			for ((j=2; j<${#list[*]}; j++)); do
				if [ "${list[j]}" == "--do" ]; then
					opt_do=$j
				fi
			done

			if [ "$opt_do" -ne -1 ]; then
				command=" ${list[opt_do+1]}"
			else
				command=" ${list[1]}"
			fi

			command+=" ${list[0]}"
			
			command+=" ${next_ip}"

			for ((j=2; j<${#list[*]}; j++)); do
				command+=" ${list[$j]}"
			done
			if [ ${result_file_default} -eq 1 ]; then
				result_files[0]="/tmp/d_${TEST_NAME}_${NOW_TS}.txt"
				command+=" -f /tmp/d_${TEST_NAME}_${NOW_TS}.txt"
			else
				result_files[0]="${RESULT_FILE}"
				command+=" -f ${RESULT_FILE}"
			fi
			
			svr_cmd=("${svr_cmd[@]}" "$command"); 
			server_ip=${list[1]}
			next_ip=${list[1]}
			break
		fi
	done


	relay_cnt=$((${#svr_inf[*]}))
	# relayサーバのコマンドリストを生成する
	for ((i=${#svr_inf[*]}; 0<i; i--)); do
		cmd=${svr_inf[$i-1]}
		list=(${cmd// / })
		if [ "${list[0]}" == "-r" ]; then
			relay_cnt=$(($relay_cnt-1))
			opt_ro=-1
			for ((j=2; j<${#list[*]}; j++)); do
				if [ "${list[j]}" == "--ro" ]; then
					opt_ro=$j
				fi
			done
			
			if [ "$opt_ro" -ne -1 ]; then
				command=" ${list[opt_ro+1]}"
			else
				command=" ${list[1]}"
			fi

			command+=" ${list[0]}"

			command+=" ${next_ip}"
			for ((j=2; j<${#list[*]}; j++)); do
				command+=" ${list[$j]}"
			done
			if [ $result_file_default -eq 1 ]; then
				result_files+=("/tmp/r${relay_cnt}_${TEST_NAME}_${NOW_TS}.txt")
				command+=" -f /tmp/r${relay_cnt}_${TEST_NAME}_${NOW_TS}.txt"
			else
				result_files+=("${RESULT_FILE}")
				command+=" -f ${RESULT_FILE}"
			fi
			svr_cmd=("${svr_cmd[@]}" "$command"); 
			next_ip=${list[1]}
		fi
	done

	# sourceサーバのコマンドリストを生成する
	for ((i=${#svr_inf[*]}; 0<i; i--)); do
		cmd=${svr_inf[$i-1]}
		list=(${cmd// / })
		if [ "${list[0]}" == "-s" ]; then
		
			opt_so=-1
			for ((j=2; j<${#list[*]}; j++)); do
				if [ "${list[j]}" == "--so" ]; then
					opt_so=$j
				fi
			done
			
			if [ "$opt_so" -ne -1 ]; then
				command=" ${list[opt_so+1]}"
			else
				command=" ${list[1]}"
			fi
			
			command+=" ${list[0]}"

			command+=" ${next_ip}"

			for ((j=2; j<${#list[*]}; j++)); do
				command+=" ${list[$j]}"
			done
			if [ $result_file_default -eq 1 ]; then
				result_files+=("/tmp/s_${TEST_NAME}_${NOW_TS}.txt")
				command+=" -f /tmp/s_${TEST_NAME}_${NOW_TS}.txt"
			else
				result_file+=("${RESULT_FILE}")
				command+=" -f ${RESULT_FILE}"
			fi
			svr_cmd=("${svr_cmd[@]}" "$command");
			client_ip=${list[1]}
			client_idx=$((${#svr_inf[*]}-1))
			break
		fi
	done

#echo "#*********** debug ***********"
#	for ((i=0; i<(${#com_inf[*]}); i++)); do
#		echo "===> com_inf[${#com_inf[*]}]: $[i] : ${com_inf[$i]}"
#	done
#echo "#*********** debug ***********"

	# update common option
	#   共通指定されたオプションが存在した場合、
	#   各サーバのコマンドオプションを共通指定された内容で更新する
	for ((i=0; i<(${#com_inf[*]}); i++)); do
		cmd=${com_inf[$i]}
		list=(${cmd// / })

		for ((j=0; j<${#list[*]}; j++ )); do
			case "${list[$j]}" in
			"-p" | "-R" | "-b" | "-B" | "-i" | "-W" | "-S" | "-R" | "-T" )
					for ((kk=0; kk<${#svr_cmd[*]}; kk++ )); do
					  update_option "${list[$j]} ${list[$j+1]}" "${svr_cmd[$kk]}"
					  svr_cmd[$kk]=${retval}
					done
				  ;;
			"-Q")
					for ((kk=0; kk<${#svr_cmd[*]}; kk++ )); do
					  update_option "${list[$j]}" "${svr_cmd[$kk]}"
					  svr_cmd[$kk]=${retval}
					  quit_flg=1
					done
				  ;;
			"-f") 
					for ((kk=0; kk<${#svr_cmd[*]}; kk++ )); do
						RESULT_FILE=${list[$j+1]}
						if [ ${kk} -eq 0 ] ; then
							RESULT_FILE="${RESULT_FILE}_d"
						elif [ ${kk} -eq $(((${#svr_cmd[*]})-1)) ] ; then
							RESULT_FILE="${RESULT_FILE}_s"
						else
							RESULT_FILE="${RESULT_FILE}_r$((${#svr_inf[*]}-${kk}-1))"
						fi
						if [ "${RESULT_FILE}" == "" -o "${RESULT_FILE:0:1}" == "-" ] ; then
							echo "[-f] result filename is empty"
							return 1
						fi
						result_path=$(dirname ${RESULT_FILE})
						if [ "${result_path}" == "." ]; then
							result_path="/tmp"
							RESULT_FILE="/tmp/${RESULT_FILE}"
						fi
						if [ "${RESULT_FILE}" == "${RESULT_FILE##*.}" ]; then
							RESULT_FILE="${RESULT_FILE}_${NOW_TS}.txt"
						else
							RESULT_FILE=${RESULT_FILE/./${NOW_TS}.}
						fi
						update_option "-f ${RESULT_FILE}" "${svr_cmd[$kk]}"
						svr_cmd[$kk]=${retval}
						result_files[kk]=${RESULT_FILE}
					done
				  ;;
			*)  
				  ;;
			esac
		done
	done
	
#echo "#*********** debug ***********"
#	for ((i=0; i<(${#svr_cmd[*]}); i++)); do
#		echo "===> command[ $[i] ]: ${svr_cmd[$i]}"
#done
#echo "#*********** debug ***********"

	return 0
}


#------------------------------------------------------------------------------
# running check
#------------------------------------------------------------------------------
function running_check()
{
	chk_ip=$1
	filename=$2
	val=""
	while [ "${val}" = "" ]
	do
		log_line_sv=$((log_line_sv+1))
		tail_command="tail -n +${log_line_sv} -F ${filename}"
		val=`${SSH_COMMAND} ${chk_ip} tail -n +${log_line_sv} -F ${filename} 2> /dev/null | start_wait`
	done
	if [ "${val}" == "" ]; then
		val="ERROR"
	fi
	echo "${val}"
}


#------------------------------------------------------------------------------
# get_logline
#------------------------------------------------------------------------------
function get_logline
{
	target_ip=$1
	filename=$2

	log_line=`${SSH_COMMAND} ${target_ip} cat ${filename} 2> /dev/null | wc -l`
	echo "${log_line}"
}


#------------------------------------------------------------------------------
# start
#  プログラムの開始位置
#------------------------------------------------------------------------------
if [ $# -eq 0 ]; then
	# 引数なしで起動された場合は、ヘルプを表示して終了する
    disp_help
    exit 0
fi

#------------------------------------------------------------------------------
# param set
#  オプションパラメータ設定処理を呼び出し
#  各サーバ毎のコマンドリストを生成する
#------------------------------------------------------------------------------
init_param $@

#------------------------------------------------------------------------------
# error check
#  オプションパラメータのチェックを行う
#------------------------------------------------------------------------------
function error_check
{
	#----------------------------------------------------------------------
	# Set MIN_RNG_BUFSIZE
	snd_length_size=131072
	for ((ii=0; ii<(${#svr_cmd[*]}); ii++)); do
		cmd=${svr_cmd[$ii]}
		list=(${cmd// / })
		MIN_RNG_BUFSIZE=$((${snd_length_size} * 2))
		for ((jj=2; jj<(${#list[*]}); jj++)); do
			if [ "${list[$jj]}" == "-b" ]; then
				snd_length_size=${list[$jj+1]}
			fi
		done
	done

	#----------------------------------------------------------------------
	# Set QuitMode
	for ((ii=0; ii<(${#svr_cmd[*]}); ii++)); do
		cmd=${svr_cmd[$ii]}
		list=(${cmd// / })
		for ((jj=2; jj<(${#list[*]}); jj++)); do
			if [ "${list[$jj]}" == "-Q" ]; then
				quit_mode=1
			fi
		done
	done

	#----------------------------------------------------------------------
	# Param Error check
	err_flg=0
	for ((ii=0; ii<(${#svr_cmd[*]}); ii++)); do
		cmd=${svr_cmd[$ii]}
		list=(${cmd// / })
		#------------------------------------------------------------------
		# IP addr check
		nc -v -w 1 ${list[0]} -z ${SSH_PORT} 2> /dev/null
		if [ $? -ne 0 ]; then
			err_flg=1
			echo "Unknown host ${list[0]} (ssh port[${SSH_PORT}] not opened)"
			break
		fi
		
		#------------------------------------------------------------------
		# Option check
		for ((jj=2; jj<(${#list[*]}); jj++)); do
			#--------------------------------------------------------------
			# [-f] check
			if [ ${quit_mode} -eq 0 ]; then
				if [ "${list[$jj]}" == "-f" ]; then
					check_val=${list[$jj+1]}
					if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
							echo "[-f] result filename is empty"
							return 1
					fi
					check_val=$(dirname ${list[$jj+1]})
					${SSH_COMMAND} ${list[0]} ls -d ${check_val} &> /dev/null
					if [ $? -ne 0 ]; then
						echo "${list[0]}: result file path error (${list[$jj+1]})"
						err_flg=1
						break
					fi
				fi
			fi
			#--------------------------------------------------------------
			# [-p] check
			if [ "${list[$jj]}" == "-p" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-p] port no is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ ${check_val} -lt ${MIN_PORT_NO} -o ${check_val} -gt ${MAX_PORT_NO}  ]; then
					echo "${list[0]}: port no error (${check_val} : ${MIN_PORT_NO}-${MAX_PORT_NO}) "
					err_flg=1
					break;
				fi
			fi
			#--------------------------------------------------------------
			# [-b] check
			if [ "${list[$jj]}" == "-b" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-b] buffer size is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ ${check_val} -lt ${MIN_BUFSIZE} -o ${check_val} -gt ${MAX_BUFSIZE}  ]; then
					echo "${list[0]}: buffer size error (${check_val} : ${MIN_BUFSIZE}-${MAX_BUFSIZE}) "
					err_flg=1
					break;
				fi
			fi
			#--------------------------------------------------------------
			# [-l] check
			if [ "${list[$jj]}" == "-l" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-l] send length is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ ${check_val} -lt ${MIN_LENGTH} ]; then
					echo "${list[0]}: send length error (${check_val} : more than ${MIN_LENGTH}) "
					err_flg=1
					break;
				fi
			fi
			#--------------------------------------------------------------
			# [-t] check
			if [ "${list[$jj]}" == "-t" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-t] start time is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ "`date +'%H:%M:%S' -d ${check_val} 2> /dev/null`" != ${check_val} ]; then
                    if [ "`date -j -f '%H:%M:%S' ${check_val} +'%H:%M:%S' 2> /dev/null`" != ${check_val} ]; then
					    echo "${list[0]}: start time error (${check_val})"
					    err_flg=1
					    break;
                    fi
				fi
			fi
			#--------------------------------------------------------------
			# [-W] check
			if [ "${list[$jj]}" == "-W" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-W] save filename is empty"
						return 1
				fi
				check_val=$(dirname ${list[$jj+1]})
				${SSH_COMMAND} ${list[0]} ls -d ${check_val} &> /dev/null
				if [ $? -ne 0 ]; then
					echo "${list[0]}: save file path error (${list[$jj+1]})"
					err_flg=1
					break
				fi
			fi
			#--------------------------------------------------------------
			# [-A] check
			if [ "${list[$jj]}" == "-A" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-A] application filename is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				chk_cmd=`${SSH_COMMAND} ${list[0]} ls ${check_val} 2> /dev/null`
				if [ -n "${chk_cmd}" ] && [ "${chk_cmd}" = "${check_val}" ]; then
					continue
				else
					echo "${list[0]}: application file not found (${list[$jj+1]})"
					err_flg=1
					break
				fi
			fi
			#--------------------------------------------------------------
			# [-C] check
			if [ "${list[$jj]}" == "-C" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-C] congestion name is empty"
						return 1
				fi
			fi
			#--------------------------------------------------------------
			# [-I] check
			if [ "${list[$jj]}" == "-I" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-I] input filename is empty"
						return 1
				fi
				check_val=$(dirname ${list[$jj+1]})
				${SSH_COMMAND} ${list[0]} ls -d ${check_val} &> /dev/null
				if [ $? -ne 0 ]; then
					echo "${list[0]}: input file path error (${list[$jj+1]})"
					err_flg=1
					break
				fi
			fi
			#--------------------------------------------------------------
			# [-O] check
			if [ "${list[$jj]}" == "-O" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-O] output filename is empty"
						return 1
				fi
				check_val=$(dirname ${list[$jj+1]})
					${SSH_COMMAND} ${list[0]} ls -d ${check_val} &> /dev/null
					if [ $? -ne 0 ]; then
						echo "${list[0]}: output file path error (${list[$jj+1]})"
						err_flg=1
						break
					fi
			fi
			#--------------------------------------------------------------
			# [-B] check
			if [ "${list[$jj]}" == "-B" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-B] buffer size is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ ${check_val} -lt ${MIN_RNG_BUFSIZE} -o ${check_val} -gt ${MAX_RNG_BUFSIZE}  ]; then
					echo "${list[0]}: ring buffer size error (${check_val} : ${MIN_RNG_BUFSIZE}-${MAX_RNG_BUFSIZE}) "
					err_flg=1
					break;
				fi
			fi
			#--------------------------------------------------------------
			# [-S] check
			if [ "${list[$jj]}" == "-S" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-S] SND_BUFSIZE is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ ${check_val} -lt ${MIN_SNDBUFSIZE} -o ${check_val} -gt ${MAX_SNDBUFSIZE}  ]; then
					echo "${list[0]}: SND_BUFSIZE error (${check_val} : ${MIN_SNDBUFSIZE}-${MAX_SNDBUFSIZE}) "
					err_flg=1
					break;
				fi
			fi
			#--------------------------------------------------------------
			# [-R] check
			if [ "${list[$jj]}" == "-R" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-R] RCV_BUFSIZE is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ ${check_val} -lt ${MIN_RCVBUFSIZE} -o ${check_val} -gt ${MAX_RCVBUFSIZE}  ]; then
					echo "${list[0]}: RCV_BUFSIZE error (${check_val} : ${MIN_RCVBUFSIZE}-${MAX_RCVBUFSIZE}) "
					err_flg=1
					break;
				fi
			fi
			#--------------------------------------------------------------
			# [-i] check
			if [ "${list[$jj]}" == "-i" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-i] interval sec is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ ${check_val} -lt ${MIN_INTERVAL} -o ${check_val} -gt ${MAX_INTERVAL}  ]; then
					echo "${list[0]}: intervel sec error (${check_val} : ${MIN_INTERVAL}-${MAX_INTERVAL}) "
					err_flg=1
					break;
				fi
			fi
			#--------------------------------------------------------------
			# [-T] check
			if [ "${list[$jj]}" == "-T" ]; then
				check_val=${list[$jj+1]}
				if [ "${check_val}" == "" -o "${check_val:0:1}" == "-" ] ; then
						echo "[-T] timeout sec is empty"
						return 1
				fi
				check_val=${list[$jj+1]}
				if [ ${check_val} -lt ${MIN_TIMEOUT} -o ${check_val} -gt ${MAX_TIMEOUT}  ]; then
					echo "${list[0]}: timeoutl sec error (${check_val} : ${MIN_TIMEOUT}-${MAX_TIMEOUT}) "
					err_flg=1
					break;
				fi
			fi
		done
	done

	if [ ${err_flg} -eq 1 ]; then
		return 1
	fi
    return 0
}
error_check
if [ $? -eq 1 ]; then
	exit
fi


#------------------------------------------------------------------------------
# run command
#   コマンドを実行する
#------------------------------------------------------------------------------
# コマンド実行後、送信ノードの実行結果を tail で出力するため
# 送信ノードのログの現在位置を取得する
log_line=`get_logline ${client_ip} ${result_files[(${#result_files[*]})-1]}`
port_skip=0


# サーバー単位でループする
for ((i=0; i<(${#svr_cmd[*]}); i++)); do
	# 最初に起動するサーバー(=受信ノード）以外の場合、サーバのポート番号を
	# コマンドに追加する
	cmd=${svr_cmd[$i]}
	if [ ${i} -ne 0 ]; then
		if [ ${port_skip} -eq 0 ]; then
			if [ ${get_port} -gt ${MAX_PORT_NO} ]; then
				echo "port no error <port=${get_port}>"
				exit
			fi
			cmd+=" -p ${get_port}"
		fi
	fi
	if [ ${i} -eq $(((${#svr_cmd[*]})-1)) ] || [ ${i} -eq 0 ] || [ ${port_skip} -eq 0 ]; then
		cmd+=" --sp ${notice_port}"
	fi

	list=(${cmd// / })
	ip_addr=${list[0]}
	command_line=${list[1]}
	for ((jj=2; jj<(${#list[*]}); jj++)); do
		command_line+=" ${list[$jj]}"
	done
	if [ ${quit_mode} -eq 0 ]; then
		log_line_sv=`get_logline ${ip_addr} ${result_files[$i]}`
	fi
	#コマンドを実行する
#	echo "${SSH_COMMAND} ${ip_addr} ${APL} ${command_line}"
	val=`${SSH_COMMAND} ${ip_addr} ${APL} ${command_line} &> /dev/null`
	ret_val=$?

	# [-Q]オプションが指定されている場合は、ポート番号の取得をスキップし終了フラグをONにする
	if [[ "${command_line}" =~ "-Q" ]]; then
		port_skip=1
		quit_flg=1

	# [-Q]オプション以外の場合は、次に実行するコマンドにポート番号を追加するため
	# ポート番号を取得する
	else
		if [ ${ret_val} -ne 0 ]; then
			echo "tcpTester does not exist <${ip_addr}>"
			exit
		fi
		# ポート番号を取得する
		get_port=`running_check ${ip_addr} ${result_files[$i]}` 
		if [ "${get_port}" == "ERROR" ]; then
			if [ ${quit_mode} -eq 0 ]; then
				disp_help	
			fi
			exit
		fi
		port_skip=0
	fi
done

# 終了フラグが立っていれば、以降のtailコマンドは行わずシェルを終了する
if [ ${quit_flg} -eq 1 ]; then
	exit
fi


#------------------------------------------------------------------------------
# tail -F log
#   実行中の経過内容を表示し続ける
#   キーワード文字(Finished or ERROR)文字の出現により表示を終了する
#------------------------------------------------------------------------------
search_word="Finished"
while :
do
	log_line=$((log_line+1))
	tail_command="${client_ip} tail -n +${log_line} -F ${result_files[(${#result_files[*]})-1]}"
		${SSH_COMMAND} ${client_ip} tail -n +${log_line} -F ${result_files[(${#result_files[*]})-1]} 2> /dev/null | auto_exit_tail
#	tail_command="tail -n +${log_line}"
#		${SSH_COMMAND} ${client_ip} tail -n +${log_line} 2> /dev/null | auto_exit_tail
	break
done

exit

