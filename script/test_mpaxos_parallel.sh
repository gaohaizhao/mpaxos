#!/bin/bash

source hosts.sh
n_tosend=1

TARGET=../bin/test_mpaxos.out
DIR_RESULT=result.mpaxos.parallel
is_exit=0
is_async=1

mkdir $DIR_RESULT &> /dev/null
#rm $DIR_RESULT/* &> /dev/null

for i in $(seq $N_HOST)
do
    ssh $USER@${MHOST[$i]} "killall test_mpaxos.out;" #rm -rf test_mpaxos/result*;"
done

n_group_begin=14
n_group_end=100
for n_group in $(seq 2000 2000)
do
    echo "TESTING FOR $n_group GROUPS"
    for i in $(seq 1 $N_HOST)
    do
        echo "LAUNCHING DAEMON $i"
        group_begin=$(expr 1000 \* $i)
        command="screen -m -d /bin/bash -c \"~/test_mpaxos/test_mpaxos.out ~/test_mpaxos/config.$N_HOST.$i 1 $n_tosend $n_group $is_async $is_exit 5 $group_begin  >& ~/test_mpaxos/result.mpaxos.$n_group.$i\"" 
        echo $command
        nohup ssh $USER@${MHOST[$i]} $command &
    done

    for i in $(seq $N_HOST)
    do
        r=""
        while [ "$r" = "" ]
        do
            command="cd ~/test_mpaxos/; cat result.mpaxos.$n_group.$i | grep \"All my task is done\""
            r=$(ssh $USER@${MHOST[$i]} "$command")
            sleep 1
        done
    done
    for i in $(seq $N_HOST)
    do
        ssh $USER@${MHOST[$i]} "killall test_mpaxos.out"
    done
done

    # fetch all the results and kill mpaxos process
    for i in $(seq $N_HOST)
    do
        ssh $USER@${MHOST[$i]} "killall test_mpaxos.out"
        #command="rm ~/test_mpaxos/*.gz; tar -czf ~/test_mpaxos/result.tar.gz ~/test_mpaxos/*"
        command="cd ~/test_mpaxos/; rm *.gz; tar -czf result.tar.gz *"
        ssh $USER@${MHOST[$i]} "$command"
        scp $USER@${MHOST[$i]}:~/test_mpaxos/result.tar.gz ./$DIR_RESULT/
        tar -xzf ./$DIR_RESULT/result.tar.gz -C ./$DIR_RESULT/
    done
