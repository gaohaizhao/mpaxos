#!/bin/bash

source hosts.sh
TARGET=../bin/test_mpaxos.out

for i in $(seq $N_HOST)
do
    ssh $USER@${MHOST[$i]} "killall test_mpaxos.out; mkdir test_mpaxos"
    scp $TARGET $USER@${MHOST[$i]}:~/test_mpaxos/
    scp ../config/config.$N_HOST.$i $USER@${MHOST[$i]}:~/test_mpaxos/
done
