.PHONY: all clean

ifndef MAKEFILE_MAIN
    $(error Config variable not defined, Please Make in project root dir)
endif

LIB_KVDB_LD_FLAGS=-ldb

LIB_KVDB_SRC=$(wildcard $(DIR_BASE)/libkvdb/*.cpp)
LIB_KVDB_OBJ=$(addprefix $(DIR_OBJ)/, $(addsuffix .o,$(basename $(notdir $(LIB_KVDB_SRC)))))

# vim: ai:ts=4:sw=4:et!

