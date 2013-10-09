#ifndef __INTERNAL_TYPES__
#define __INTERNAL_TYPES__

#include "mpaxos/mpaxos-types.h"
#include "mpaxos.pb-c.h"
#include "utils/safe_assert.h"

#define MAJORITY_UNCERTAIN 0
#define MAJORITY_YES 1
#define MAJORITY_NO 2

#define BID_PRIOR 1
#define BID_NORMAL 2

typedef Mpaxos__InstidT instid_t;
typedef Mpaxos__RoundidT roundid_t;
typedef Mpaxos__AckEnum ack_enum;
typedef Mpaxos__ResponseT response_t;
typedef Mpaxos__MsgCommon msg_common_t;
typedef Mpaxos__MsgPromise msg_promise_t;
typedef Mpaxos__MsgPrepare msg_prepare_t;
typedef Mpaxos__Proposal proposal_t;
typedef Mpaxos__MsgAccept msg_accept_t;
typedef Mpaxos__MsgAccepted msg_accepted_t;
typedef Mpaxos__MsgLearn msg_learn_t;
typedef Mpaxos__MsgDecide msg_decide_t;
typedef Mpaxos__MsgSlot msg_slot_t;
typedef Mpaxos__MsgHeader msg_header_t;
typedef Mpaxos__ProcessidT processid_t;

typedef uint8_t msg_type_t;


typedef struct {
    roundid_t rid;
    apr_hash_t *promise_ht;
    apr_hash_t *accepted_ht;
    uint32_t n_promises_yes;
    uint32_t n_promises_no;
    uint32_t n_accepteds_yes;
    uint32_t n_accepteds_no;
    proposal_t *max_bid_prop_ptr;
    pthread_mutex_t mutex;
} group_info_t;

typedef struct {
    groupid_t* gids;
    size_t sz_gids;
    slotid_t *sids;
    uint8_t *data;
    size_t sz_data;
    void* cb_para;
    uint32_t n_retry;
    apr_time_t tm_start;
    apr_time_t tm_end;
} mpaxos_req_t;

typedef struct {
    roundid_t *rid;
    apr_pool_t *mp;
    apr_hash_t *group_info_ht;  //groupid_t -> group_info_t
    apr_thread_mutex_t *mx;
//    apr_thread_cond_t *cond_prep;
//    apr_thread_cond_t *cond_accp;
    mpaxos_req_t * req;
    roundid_t **rids;
    size_t sz_rids;
    int after_phase1; // 1 is true
    int after_phase2; // 1 is true
    int is_voriginal; // 0 is true
    int is_good;      // 0 is true. Fuck!
    proposal_t *prop_max;
    proposal_t *prop_self;
    
} round_info_t;


static void prop_destroy(proposal_t *prop) {
//  for (int i = 0; i < prop->n_rids; i++) {
//      free(prop->rids[i]);
//  }
//  free(prop->rids);
//  free(prop->value.data);
    free(prop);
}

static void prop_cpy(proposal_t *dest, const proposal_t *src, apr_pool_t *mp) {
    SAFE_ASSERT(mp != NULL);
    SAFE_ASSERT(src != NULL);
    SAFE_ASSERT(dest != NULL);
    mpaxos__proposal__init(dest);
    dest->nid = src->nid;
    dest->n_rids = src->n_rids;
    dest->rids = apr_pcalloc(mp, sizeof(instid_t*) * src->n_rids);
    int i;
    for (i = 0; i < src->n_rids; i++) {
        dest->rids[i] = (roundid_t *)apr_pcalloc(mp, sizeof(roundid_t));
        mpaxos__roundid_t__init(dest->rids[i]);
        dest->rids[i]->gid = src->rids[i]->gid;
        dest->rids[i]->sid = src->rids[i]->sid;
        dest->rids[i]->bid = src->rids[i]->bid;
    }
    dest->value.len = src->value.len;
    dest->value.data = (uint8_t *)apr_pcalloc(mp, src->value.len);
    memcpy(dest->value.data, src->value.data, src->value.len);
}

#endif
