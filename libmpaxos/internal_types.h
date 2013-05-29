#ifndef __INTERNAL_TYPES__
#define __INTERNAL_TYPES__

#include "mpaxos/mpaxos-types.h"
#include "mpaxos.pb-c.h"

typedef Mpaxos__InstidT instid_t;
typedef Mpaxos__RoundidT roundid_t;
typedef Mpaxos__Proposal proposal;
typedef Mpaxos__AckEnum ack_enum;
typedef Mpaxos__ResponseT response_t;
typedef Mpaxos__MsgPromise msg_promise_t;
typedef Mpaxos__MsgPrepare msg_prepare_t;
typedef Mpaxos__MsgAccept msg_accept_t;
typedef Mpaxos__MsgLearn msg_learn_t;
typedef Mpaxos__MsgSlot msg_slot_t;

static void prop_destroy(proposal *prop) {
//  for (int i = 0; i < prop->n_rids; i++) {
//      free(prop->rids[i]);
//  }
//  free(prop->rids);
//  free(prop->value.data);
    free(prop);
}

static void prop_cpy(proposal *dest, const proposal *src, apr_pool_t *pool) {
//  mpaxos__proposal__init(p);
    dest->n_rids = src->n_rids;
    dest->rids = apr_pcalloc(pool, sizeof(instid_t*) * src->n_rids);
    int i;
    for (i = 0; i < src->n_rids; i++) {
        dest->rids[i] = (roundid_t *)apr_pcalloc(pool, sizeof(roundid_t));
        dest->rids[i]->gid = src->rids[i]->gid;
        dest->rids[i]->sid = src->rids[i]->gid;
        dest->rids[i]->bid = src->rids[i]->bid;
    }
    dest->value.len = src->value.len;
    dest->value.data = (uint8_t *)apr_pcalloc(pool, src->value.len);
    memcpy(dest->value.data, src->value.data, src->value.len);
}

#endif
