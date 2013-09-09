#!/bin/bash

N_HOST=5
TARGET=../bin/test_mpaxos.out
n_group=10
n_tosend=10

DIR_RESULT=result.mpaxos.debug

killall test_mpaxos.out &> /dev/null
mkdir $DIR_RESULT &> /dev/null
rm $DIR_RESULT/* &> /dev/null
for n_group in $(seq 100 100)
do
    killall test_mpaxos.out &> /dev/null
    sleep 5
    echo "TESTING FOR $n_group GROUPS"
    for i in $(seq 2 $N_HOST)
    do
        echo "LAUNCHING DAEMON $i"
        command="$TARGET ../config/config.$N_HOST.$i 0  >& ./$DIR_RESULT/result.mpaxos.$N_HOST.$n_group.$i"
        #echo $command
        #screen -m -d /bin/bash -c "$command"
        command_stdout="$TARGET ../config/config.$N_HOST.$i 0"
        nohup xterm -hold -e "$command_stdout" &

#        xterm "$TARGET ../config/config.$N_HOST.$i 0" #>& ./$DIR_RESULT/result.mpaxos.$N_HOST.$i.$n_group"
#        command="$TARGET ../config/config.$N_HOST.$i 0 >& ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group"
#        echo $command
    done
    
    # the master
    i=1
    echo "START MASTER"
    command="$TARGET ../config/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5 >& ./$DIR_RESULT/result.mpaxos.$N_HOST.$n_group.$i"
    #echo $command
    #/bin/bash -c "$command"
    command_stdout="$TARGET ../config/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5"
    nohup xterm -hold -e "$command_stdout" &
    #$TARGET ../config/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5  >& ./$DIR_RESULT/result.mpaxos.$N_HOST.$n_group.$i
    #mutrace $TARGET ../config/config.$N_HOST.$i 1 100 $n_group 0 1 5 2>tmp # 1> ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group
    #valgrind --tool=drd --exclusive-threshold=100 $TARGET ../config/config.$N_HOST.$i 1 100 $n_group 0 1 5  2>tmp #1> ./result.mpaxos/result.mpaxos.$N_HOST.$i.$n_group
done
