#!/bin/bash

source target.sh

TARGET=../bin/$TEST_MPAXOS

screen -m -d /bin/bash -c "$TARGET ../config/config.3.1 1 10 10 >& tmp1"
screen -m -d /bin/bash -c "$TARGET ../config/config.3.2 1 10 10 >& tmp2"
screen -m -d /bin/bash -c "$TARGET ../config/config.3.3 1 10 10 >& tmp3"
