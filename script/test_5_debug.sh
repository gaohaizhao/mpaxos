#!/bin/sh
screen -m -d ./test_mpaxos config.5.2 0 0 0 > tmp2
screen -m -d ./test_mpaxos config.5.3 0 0 0 > tmp3
screen -m -d ./test_mpaxos config.5.4 0 0 0 > tmp4
screen -m -d ./test_mpaxos config.5.5 0 0 0 > tmp5
sleep 1
#./test_mpaxos config.5.1 1 5000 10 >tmp1
#killall test_mpaxos
