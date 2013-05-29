#!/bin/bash

source target.sh
TARGET=../bin/$TEST_MPAXOS

screen -m -d /bin/bash -c "$TARGET ../config/config.5.1 1 50 1 >& tmp1"
screen -m -d /bin/bash -c "$TARGET ../config/config.5.2 1 50 1 >& tmp2"
screen -m -d /bin/bash -c "$TARGET ../config/config.5.3 1 50 1 >& tmp3"
screen -m -d /bin/bash -c "$TARGET ../config/config.5.4 1 50 1 >& tmp4"
screen -m -d /bin/bash -c "$TARGET ../config/config.5.5 1 50 1 >& tmp5"
