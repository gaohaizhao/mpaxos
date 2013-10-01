#!/bin/bash

source hosts.sh

for i in $(seq $N_HOST)
do
    ssh $USER@${MHOST[$i]} "killall test_mpaxos.out; rm -rf test_mpaxos/result*;"
done
