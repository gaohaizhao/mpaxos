#!/bin/bash

source hosts.sh
n_tosend=1

TARGET=../bin/test_mpaxos.out
DIR_RESULT=result.mpaxos.livelock.client
is_exit=0
is_async=1
n_group=1
sz_data=100
n_batch=10

mkdir $DIR_RESULT &> /dev/null
rm $DIR_RESULT/* &> /dev/null

for i in $(seq $N_HOST)
do
    ssh $USER@${MHOST[$i]} "rm -rf test_mpaxos"
    ssh $USER@${MHOST[$i]} "killall test_mpaxos.out"
done

for i in $(seq $N_HOST)
do
    ssh $USER@${MHOST[$i]} mkdir test_mpaxos
    scp $TARGET $USER@${MHOST[$i]}:~/test_mpaxos/
    scp ../config/config.$N_HOST.$i $USER@${MHOST[$i]}:~/test_mpaxos/
done

for n_client in $(seq 1 5)
do
    echo "TESTING FOR $n_group GROUPS"
    for i in $(seq 1 $N_HOST)
    do
        echo "LAUNCHING DAEMON $i"
        #group_begin=$(expr 1000 \* $i)
        group_begin=1000
        sz_data=$(expr 100 \* $n_batch)
        run=0 
        if [ $i -le $n_client ]; then
            run=1 
        fi
        command="screen -m -d /bin/bash -c \"~/test_mpaxos/test_mpaxos.out ~/test_mpaxos/config.$N_HOST.$i $run $n_tosend $n_group $is_async $is_exit 5 $group_begin $n_batch $sz_data  >& ~/test_mpaxos/result.mpaxos.$n_client.$i\"" 
        echo $command
        ssh $USER@${MHOST[$i]} $command
    done

    for i in $(seq $n_client)
    do
        r=""
        while [ "$r" = "" ]
        do
            command="cd ~/test_mpaxos/; cat result.mpaxos.$n_client.$i | grep \"All my task is done\""
            r=$(ssh $USER@${MHOST[$i]} "$command")
            sleep 1
        done
    done
    
    # fetch all the results and kill mpaxos process
    for i in $(seq $N_HOST)
    do
        ssh $USER@${MHOST[$i]} "killall test_mpaxos.out"
        scp $USER@${MHOST[$i]}:~/test_mpaxos/result.mpaxos.$n_client.$i ./$DIR_RESULT/
    done
done
