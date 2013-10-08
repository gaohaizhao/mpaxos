#include "mpaxos/mpaxos.h"
#include "internal_types.h"

void handle_msg_decide(msg_decide_t *msg_dcd);

void decide_value(roundid_t **rids, size_t sz_rids, 
        uint8_t *data, size_t sz_data);