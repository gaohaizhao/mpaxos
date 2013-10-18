
#include "mpaxos/mpaxos.h"
#include "internal_types.h"
#include "learner.h"
#include "proposer.h"
#include "acceptor.h"
#include "recorder.h"


void handle_msg_decide(msg_decide_t *msg_dcd) {
/*
    roundid_t **rids = msg_dcd->prop->rids;
    decide_value(msg_dcd->prop->rids, msg_dcd->prop->n_rids, 
            msg_dcd->prop->value.data, msg_dcd->prop->value.len);
*/
    record_proposal(msg_dcd->prop);
}


// TODO [fix]
void decide_value(roundid_t **rids, size_t sz_rids, 
        uint8_t *data, size_t sz_data) {    
    //  remember the decided value.
/*
    for (int i = 0; i < sz_rids; i++) {
        roundid_t *rid = rids[i];
        groupid_t gid = rid->gid;
        slotid_t sid = rid->sid;
        put_instval(gid, sid, data, sz_data);
        // TODO [fix] callback.
        
//        rinfo->req->sids[i] = rid->sid;
    }
*/
}
