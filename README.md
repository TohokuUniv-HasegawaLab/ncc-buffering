# Overview
This repository contains the source code and executable files of an application developed for data transfer and relay.

# Directory Structure
- source/
  - main.c
  - common.c
  - init.c
  - global.c
  - help.c
  - output.c
  - tcpTester.h
  - Makefile
- ttManager.sh

# Build Instructions

```bash
cd source
make clean
make
```
# Usage
```bash
ttManager.sh [-s host | -r host | -d] [option]
ttManager.sh [-h]

Souce or Relay or Destination
  -f <test_filename>       test file name (default=/tmp/TXXXXX_YYYYMDD_HHMMSS.txt)
  -b <nnnn>                buffer size (default=128K byte)
                             setting range=1-1048576
  -E <ssh command>         ssh command
                             ex) ttManager.sh  -E "sshpassh -p password ssh"
  -i <nn>                  reports interval(default=1s)
                             setting ragen=1-60
  -p <nnnn>                destination port to listen on/connect to (default=auto select)
                             setting range=20000-29999
  -T <nn>                  send or recv timeout sec (default=30s)
                             setting range=0-1800
  -h                       show help message
  -Q                       forced termination

Souce specific:
  -s <host>                run in send node
  -l <nnnn>                data length (default=2Gbyte)
                             setting range=more than 1
  -t <HH:MM:SS>            time to start sending
  -C <algo>                set TCP congestion control algorithm(algo=cubic, reno, ccp)
  -S <nnnn>                send buffer size(default=max buffersize)
                             setting renge=4096-4194304
                             reference) /proc/sys/net/ipv4/tcp_wmem => min buffersize-max maffersize
  --so <host>              outbound send node
  --DEBUG                  debuging log output

Relay specific:
  -r <host>                run in relay node
  -C <algo>                set TCP congestion control algorithm(algo=cubic, reno, ccp)
  -A <apl filename>        aplication file name
  -I <apl input filename>  aplication input file name(default=input.dat)
  -O <apl output filename> aplication output file name(default=output.dat)
  -B <nnnn>                ring buffer size(default=250M)
                             setting range=more than buffer_size(-b) * 2
  -S <nnnn>                send buffer size(default=max buffersize)
                             setting renge=4096-4194304
                             reference) /proc/sys/net/ipv4/tcp_wmem
  -R <nnnn>                recv buffer size(default=max buffersize)
                             setting renge=4096-5666432
                             reference) /proc/sys/net/ipv4/tcp_rmem
  --ro <host>              outbound relay node
  --DEBUG                  debuging log output

Destination specific:
  -d <host>                run in recv node
  -W <save filename>       file name for receive data
  -R <nnnn>                recv buffer size(default=max buffersize)
                             setting renge=4096-5666432
                             reference) /proc/sys/net/ipv4/tcp_rmem
  --do <host>              outbound recv node
  --DEBUG                  debuging log output
```

# Note
Tested only in limited environments.

If you have any questions, please contact me at [tabuchi.yuki.p7@dc.tohoku.ac.jp]
