#!/bin/bash

source target.sh

TARGET=../bin/$TEST_MPAXOS

screen -m -d /bin/bash -c "$TARGET ../config/config.2.1 1 100 1 > tmp1"
screen -m -d /bin/bash -c "$TARGET ../config/config.2.2 1 100 1 > tmp2"
