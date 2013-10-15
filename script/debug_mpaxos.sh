#!/bin/bash

N_HOST=3
TARGET=../bin/test_mpaxos.out
n_group=10000
n_tosend=1000
is_exit=0
is_async=1
n_batch=10
to_sleep=5

DIR_CONFIG=../config/config.local/
DIR_RESULT=result.mpaxos.debug

killall test_mpaxos.out &> /dev/null
killall xterm &> /dev/null

mkdir $DIR_RESULT &> /dev/null
rm $DIR_RESULT/* &> /dev/null
for n_group in $(seq 1 1)
do
    killall test_mpaxos.out &> /dev/null
    sleep 5
    echo "TESTING FOR $n_group GROUPS"
    for i in $(seq 1 $N_HOST)
    do
        echo "LAUNCHING DAEMON $i"
        command="$TARGET ../config/config.$N_HOST.$i 0  >& ./$DIR_RESULT/result.mpaxos.$N_HOST.$n_group.$i"
        #echo $command
        group_begin=$(expr 1000 \* $i) 
        group_begin=1000
        to_sleep=5
        command_stdout="$TARGET $DIR_CONFIG/config.$N_HOST.$i 1 $n_tosend $n_group $is_async $is_exit $to_sleep $group_begin $n_batch"
        nohup xterm -hold -e "$command_stdout" &
        #screen -m -d /bin/bash -c "$command_stdout"
    done
    
    # the master
    #i=1
    #echo "START MASTER"
    #command="$TARGET ../config/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5 >& ./$DIR_RESULT/result.mpaxos.$N_HOST.$n_group.$i"
    #echo $command
    #/bin/bash -c "$command"
    #command_stdout="$TARGET ../config/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5"
    #nohup xterm -hold -e "$command_stdout" &
    #$TARGET ../config/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5  >& ./$DIR_RESULT/result.mpaxos.$N_HOST.$n_group.$i
    #mutrace $TARGET ../config/config.$N_HOST.$i 1 100 $n_group 0 1 5 2>tmp # 1> ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group
    #valgrind --tool=drd --exclusive-threshold=100 $TARGET ../config/config.$N_HOST.$i 1 100 $n_group 0 1 5  2>tmp #1> ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group
done
