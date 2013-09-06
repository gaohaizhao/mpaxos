#!/bin/bash

source hosts.sh
n_tosend=10

TARGET=../bin/test_mpaxos.out
DIR_RESULT=result.mpaxos.parallel

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

for n_group in $(seq 1000)
do
    echo "TESTING FOR $n_group GROUPS"
    for i in $(seq 2 $N_HOST)
    do
        echo "LAUNCHING DAEMON $i"
        command="screen -m -d /bin/bash -c \"~/test_mpaxos/test_mpaxos.out ~/test_mpaxos/config.$N_HOST.$i 0 >& ~/test_mpaxos/result.mpaxos.$N_HOST.$i.$n_group\""
        echo $command
        ssh $USER@${MHOST[$i]} $command
    done
    
    # the master
    echo "LAUNCHING MASTER"
    i=1
    command="~/test_mpaxos/test_mpaxos.out ~/test_mpaxos/config.$N_HOST.$i 1 $n_tosend $n_group 1 1 5  >& ~/test_mpaxos/result.mpaxos.$N_HOST.$i.$n_group"
    ssh $USER@${MHOST[$i]} $command
    
    # fetch all the results and kill mpaxos process
    mkdir result.mpaxos
    for i in $(seq $N_HOST)
    do
        scp $USER@${MHOST[$i]}:~/test_mpaxos/result.mpaxos.$N_HOST.$i.$n_group ./$DIR_RESULT/
        killall test_mpaxos.out
        
    done
done
