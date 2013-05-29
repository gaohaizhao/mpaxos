.PHONY: all clean

ifndef MAKEFILE_MAIN
    $(error Config variable not defined, Please Make in project root dir)
endif

-include mpaxos.mk

all: $(LIB_MPAXOS_OBJ)

$(DIR_OBJ)/%.o: %.c
	$(V)echo "[ CC ] $<"
	$(V)$(CC) -I. -I$(DIR_INC) $(CCFLAGS) $(LIB_MPAXOS_CC_FLAGS) $< -o $@

clean:
	-rm $(LIB_MPAXOS_OBJ)

# vim: ai:ts=4:sw=4:et!
