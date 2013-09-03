#!/bin/bash

source hosts.sh



for i in $(seq $N_HOST)
do
    ssh $USER@${MHOST[$i]} mkdir test_mpaxos
    scp ../bin/test_mpaxos.out $USER@${MHOST[$i]}:~/test_mpaxos/
    scp ../config/config.$N_HOST.$i $USER@${MHOST[$i]}:~/test_mpaxos/
done


for n_group in $(seq 100)
do
    echo "TESTING FOR $n_group GROUPS"
    for i in $(seq 2 $N_HOST)
    do
        command="screen -m -d /bin/bash -c \"~/test_mpaxos/test_mpaxos.out ~/test_mpaxos/config.$N_HOST.$i 0 100 >& ~/test_mpaxos/result.mpaxos.$N_HOST.$i.$n_group\""
        echo $command
        ssh $USER@${MHOST[$i]} $command
    done
    
    # the master
    i=1
    command="~/test_mpaxos/test_mpaxos.out ~/test_mpaxos/config.$N_HOST.$i 1 100 $n_group 1 1 5 >& ~/test_mpaxos/result.mpaxos.$N_HOST.$i.$n_group"
    ssh $USER@${MHOST[$i]} $command
    
    
    # fetch all the results and kill mpaxos process
    mkdir result.mpaxos
    for i in $(seq $N_HOST)
    do
        scp $USER@${MHOST[$i]}:~/test_mpaxos/result.mpaxos.$N_HOST.$i.$n_group ./result.mpaxos/
        killall test_mpaxos.out
        rm ~/test_mpaxos/result*
    done
done
