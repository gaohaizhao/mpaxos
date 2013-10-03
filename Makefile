.PHONY: all mpaxos test kvdb clean

export VERSION_MAJOR=1
export VERSION_MINOR=0
export VERSION_REVISION=$(shell git log -1 --decorate=no --abbrev=6 --no-notes --no-color --format="%h")

export debug ?= y
export V ?= @
export DIR_BASE ?= ${PWD}
export DIR_OBJ  ?= ${PWD}/obj
export DIR_BIN  ?= ${PWD}/bin
export DIR_INC  ?= ${PWD}/include

export MAKEFILE_MAIN ?= 1

export COMMA:= ,
export EMPTY:=
export SPACE:=$(EMPTY) $(EMPTY)

#export CC=gcc
#export CXX=g++
#export LD=gcc
#export LDXX=c++

export CC=clang
export CXX=clang++
export LD=clang
export LDXX=clang++

ifeq ($(debug),y)
    export CCFLAGS=-std=c99 -O0 -g -pg -c -Wall -Wno-unused -rdynamic
    export CXXFLAGS=-std=c++11 -O0 -g -pg -c -Wall -Wno-unused
    export LDFLAGS=-O0 -g -p -pg -rdynamic
	export VERSION_SUFFIX=.out
else
    export CCFLAGS=-std=c99 -O2 -c -Wall -Wno-unused
    export CXXFLAGS=-std=c++11 -O2 -c -Wall -Wno-unused
    export LDFLAGS=-O2
	export VERSION_SUFFIX=-$(shell uname -m)-$(VERSION_MAJOR).$(VERSION_MINOR)-$(VERSION_REVISION).out
endif

all: mpaxos test kvdb

$(DIR_BIN):
	$(V)echo "[ MD ] $@"
	$(V)mkdir -p $@

$(DIR_OBJ):
	$(V)echo "[ MD ] $@"
	$(V)mkdir -p $@

mpaxos: $(DIR_BIN) $(DIR_OBJ)
	$(V)make -C libmpaxos

kvdb: mpaxos
	$(V)make -C libkvdb
	$(V)make -C test kvdb

test_%: test

test: mpaxos kvdb
	$(V)make -C test

clean:
	-rm -rf $(DIR_OBJ)
	-rm -rf $(DIR_BIN)

# vim: ai:ts=4:sw=4:et!
