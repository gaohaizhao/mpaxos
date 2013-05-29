.PHONY: all clean

ifndef MAKEFILE_MAIN
    $(error Config variable not defined, Please Make in project root dir)
endif

-include ../libmpaxos/mpaxos.mk
-include ../libkvdb/kvdb.mk

tests=test_mpaxos test_kvdb
test_bin=$(addsuffix $(VERSION_SUFFIX), $(addprefix $(DIR_BIN)/, $(tests)))

all: $(test_bin)

kvdb: $(DIR_BIN)/test_kvdb$(VERSION_SUFFIX)

mpaxos: $(DIR_BIN)/test_mpaxos$(VERSION_SUFFIX)

$(DIR_BIN)/test_mpaxos$(VERSION_SUFFIX): $(DIR_OBJ)/test_mpaxos.o $(LIB_MPAXOS_OBJ)
	$(V)echo "[ LD ] $@"
	$(V)$(LD) $^ $(LIB_MPAXOS_LD_FLAGS) -o $@

$(DIR_BIN)/test_kvdb$(VERSION_SUFFIX): $(DIR_OBJ)/test_kvdb.o $(LIB_MPAXOS_OBJ) $(LIB_KVDB_OBJ)
	$(V)echo "[ LD ] $@"
	$(V)$(LDXX) $^ $(LIB_MPAXOS_LD_FLAGS) $(LIB_KVDB_LD_FLAGS) -o $@

$(DIR_OBJ)/%.o: %.c
	$(V)echo "[ CC ] $@"
	$(V)$(CC) -I. -I$(DIR_INC) $(CCFLAGS) $(LIB_MPAXOS_CC_FLAGS) $< -o $@

# vim: ai:ts=4:sw=4:et!
