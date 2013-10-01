#!/bin/bash

ARCH=$(uname -m)
VERSION_MAJOR=1
VERSION_MINOR=0
VERSION_REVISION=$(git log -1 --decorate=no --abbrev=6 --no-notes --no-color --format="%h")

#TEST_MPAXOS=test_mpaxos-$ARCH-$VERSION_MAJOR.$VERSION_MINOR-$VERSION_REVISION.out
TEST_MPAXOS=test_mpaxos.out
