#!/bin/bash

N_HOST=5
TARGET=../bin/test_mpaxos.out
n_group=10
n_tosend=10

mkdir result.mpaxos &> /dev/null
rm result.mpaxos/* &> /dev/null
for n_group in $(seq 1 20)
do
    killall test_mpaxos.out &> /dev/null
    sleep 5
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
    command="$TARGET ../config/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5 >& ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group"
    echo $command
    $TARGET ../config/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5 #>& ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group
    #mutrace $TARGET ../config/config.$N_HOST.$i 1 100 $n_group 0 1 5 2>tmp # 1> ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group
    #valgrind --tool=drd --exclusive-threshold=100 $TARGET ../config/config.$N_HOST.$i 1 100 $n_group 0 1 5  2>tmp #1> ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group
done
