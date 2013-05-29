#!/bin/bash

source target.sh

USER=ms

HOST1=localhost
HOST2=localhost
HOST3=localhost

#ssh-copy-id $USER@$HOST1
#ssh-copy-id $USER@$HOST2
#ssh-copy-id $USER@$HOST3

TARGET=../bin/$TEST_MPAXOS

CONFIG1=../config/config.3.1
CONFIG2=../config/config.3.2
CONFIG3=../config/config.3.3

ssh $USER@$HOST1 mkdir mpaxos
ssh $USER@$HOST2 mkdir mpaxos
ssh $USER@$HOST3 mkdir mpaxos

scp $TARGET $USER@$HOST1:~/mpaxos/
scp $TARGET $USER@$HOST2:~/mpaxos/
scp $TARGET $USER@$HOST3:~/mpaxos/

scp $CONFIG1 $USER@$HOST1:~/mpaxos/
scp $CONFIG2 $USER@$HOST1:~/mpaxos/
scp $CONFIG3 $USER@$HOST1:~/mpaxos/


TARGET=~/mpaxos/$TEST_MPAXOS
# do
COMMAND1="screen -m -d /bin/bash -c \"$TARGET ~/mpaxos/config.3.1 1 100 3 >& ~/mpaxos/tmp1\""
COMMAND2="screen -m -d /bin/bash -c \"$TARGET ~/mpaxos/config.3.2 1 100 3 >& ~/mpaxos/tmp2\""
COMMAND3="screen -m -d /bin/bash -c \"$TARGET ~/mpaxos/config.3.3 1 100 3 >& ~/mpaxos/tmp3\""

echo $COMMAND1
echo $COMMAND2
echo $COMMAND3

ssh $USER@$HOST1 $COMMAND1
ssh $USER@$HOST2 $COMMAND2
ssh $USER@$HOST3 $COMMAND3


# fetch all the results
#scp $USER@$HOST1:~/tmp1 ./
#scp $USER@$HOST2:~/tmp2 ./
#scp $USER@$HOST3:~/tmp3 ./


# clean
# ssh ($USER)@($HOST1):~/tmp1
# ssh ($USER)@($HOST2):~/tmp2
# ssh ($USER)@($HOST3):~/tmp3
