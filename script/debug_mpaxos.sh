#!/bin/bash

N_HOST=5
TARGET=../bin/test_mpaxos.out
n_group=10

rm result.mpaxos/*
killall test_mpaxos.out &> /dev/null
for n_group in $(seq 1 100)
do
    echo "TESTING FOR $n_group GROUPS"
    for i in $(seq 2 $N_HOST)
    do
        echo "LAUNCHING DAEMON $i"
        screen -m -d /bin/bash -c "$TARGET ../config/config.$N_HOST.$i 0 >& ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group"
#        command="$TARGET ../config/config.$N_HOST.$i 0 >& ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group"
#        echo $command
    done
    
    # the master
    i=1
    echo "START MASTER"
    $TARGET ../config/config.$N_HOST.$i 1 100 $n_group 0 1 5 >& ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group
    
    killall test_mpaxos.out
done
