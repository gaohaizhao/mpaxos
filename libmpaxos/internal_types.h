#ifndef __INTERNAL_TYPES__
#define __INTERNAL_TYPES__

#include "mpaxos/mpaxos-types.h"
#include "mpaxos.pb-c.h"

typedef Mpaxos__InstidT instid_t;
typedef Mpaxos__RoundidT roundid_t;
typedef Mpaxos__Proposal proposal;
typedef Mpaxos__AckEnum ack_enum;
typedef Mpaxos__ResponseT response_t;
typedef Mpaxos__MsgCommon msg_common_t;
typedef Mpaxos__MsgPromise msg_promise_t;
typedef Mpaxos__MsgPrepare msg_prepare_t;
typedef Mpaxos__Proposal proposal_t;
typedef Mpaxos__MsgAccept msg_accept_t;
typedef Mpaxos__MsgAccepted msg_accepted_t;
typedef Mpaxos__MsgLearn msg_learn_t;
typedef Mpaxos__MsgSlot msg_slot_t;
typedef Mpaxos__MsgHeader msg_header_t;
typedef Mpaxos__ProcessidT processid_t;


typedef struct {
    roundid_t rid;
    apr_hash_t *promise_ht;
    apr_hash_t *accepted_ht;
    uint32_t n_promises;
    uint32_t n_accepteds;
    proposal *max_bid_prop_ptr;
    pthread_mutex_t mutex;
} group_info_t;

typedef struct {
    groupid_t* gids;
    size_t sz_gids;
    slotid_t *sids;
    uint8_t *data;
    size_t sz_data;
    void* cb_para;
} mpaxos_req_t;

typedef struct {
    roundid_t *rid;
    apr_pool_t *round_pool;
    apr_hash_t *group_info_ht;  //groupid_t -> group_info_t
    apr_thread_mutex_t *mx;
//    apr_thread_cond_t *cond_prep;
//    apr_thread_cond_t *cond_accp;
    mpaxos_req_t * req;
    roundid_t **rids;
    size_t sz_rids;
    int after_phase1;
    int after_phase2;
} round_info_t;


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
