.PHONY: all clean

ifndef MAKEFILE_MAIN
    $(error Config variable not defined, Please Make in project root dir)
endif

-include ../libmpaxos/mpaxos.mk
-include ./kvdb.mk

all: $(LIB_KVDB_OBJ)

$(DIR_OBJ)/%.o: %.cpp
	$(V)echo "[ CC ] $<"
	$(V)$(CXX) -I. -I$(DIR_INC) $(CXXFLAGS) $(LIB_MPAXOS_CC_FLAGS) $(LIB_KVDB_CC_FLAGS) $< -o $@

clean:
	-rm $(LIB_KVDB_OBJ)

# vim: ai:ts=4:sw=4:et!
