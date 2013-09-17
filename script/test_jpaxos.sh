#!/bin/bash


source hosts.sh
n_tosend=10

TARGET=./testjpaxos.jar
DIR_RESULT=result.jpaxos.base

N_HOST=5

mkdir $DIR_RESULT &> /dev/null
rm $DIR_RESULT/* &> /dev/null

for i in $(seq $N_HOST)
do
    ssh $USER@${MHOST[$i]} "rm -rf test_jpaxos/*"
    ssh $USER@${MHOST[$i]} "killall screen"
#    ssh $USER@${MHOST[$i]} "rm -rf ~/test_jpaxos/jpaxosLogs &> /dev/null"
done

for i in $(seq $N_HOST)
do
    ssh $USER@${MHOST[$i]} mkdir test_jpaxos
    scp $TARGET $USER@${MHOST[$i]}:~/test_jpaxos/
    scp ./paxos.properties.remote $USER@${MHOST[$i]}:~/test_jpaxos/paxos.properties
done

for i in $(seq 1 4)
do
    command="cd test_jpaxos; java -jar ./testjpaxos.jar $i 0 &> result.jpaxos.$i"
    ssh $USER@${MHOST[$i]} "screen -m -d -L /bin/bash -c \"$command\""
done

    i=0
    command="cd test_jpaxos; java -jar ./testjpaxos.jar $i $n_tosend &> result.jpaxos.$i"
    #ssh $USER@${MHOST[$i]} "$command"
    ssh $USER@${MHOST[$i]} "screen -m -d -L /bin/bash -c \"$command\""

    while [ "$r" = "" ]
    do
        sleep 1
        echo "sleep 1s to wait for results"
        command="cd ~/test_jpaxos/; cat result.jpaxos.0 | grep \"all done\""
        r=$(ssh $USER@${MHOST[$i]} "$command")
    done
    echo $r

for i in $(seq 0 4)
do
    scp $USER@${MHOST[$i]}:~/test_jpaxos/result.jpaxos.$i ./$DIR_RESULT/
    ssh $USER@${MHOST[$i]} "killall java"
done

