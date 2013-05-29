#!/bin/sh
screen -m -d ./test_mpaxos config.3.2 0 0 0 > tmp2
screen -m -d ./test_mpaxos config.3.3 0 0 0 > tmp3
#sleep 1
#./test_mpaxos config.3.1 1 10000 1 > tmp1
#killall test_mpaxos
