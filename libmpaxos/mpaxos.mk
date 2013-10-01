.PHONY: all clean

ifndef MAKEFILE_MAIN
    $(error Config variable not defined, Please Make in project root dir)
endif

LIB_MPAXOS_LIBS= json apr-1 apr-util-1 libprotobuf-c
LIB_MPAXOS_CC_FLAGS=$(addprefix `pkg-config --cflags , $(addsuffix `,$(LIB_MPAXOS_LIBS)))
LIB_MPAXOS_LD_FLAGS=$(addprefix `pkg-config --libs , $(addsuffix `,$(LIB_MPAXOS_LIBS)))

LIB_MPAXOS_SRC=$(wildcard $(DIR_BASE)/libmpaxos/*.c)
LIB_MPAXOS_OBJ=$(addprefix $(DIR_OBJ)/, $(addsuffix .o,$(basename $(notdir $(LIB_MPAXOS_SRC)))))

# vim: ai:ts=4:sw=4:et!

