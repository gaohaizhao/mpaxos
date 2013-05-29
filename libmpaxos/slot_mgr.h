#include <stdbool.h>
#include "mpaxos/mpaxos.h"

void slot_mgr_init();

void slot_mgr_destroy();

bool is_slot_mgr();

slotid_t alloc_slot(groupid_t, nodeid_t);

slotid_t acquire_slot(groupid_t, nodeid_t);
