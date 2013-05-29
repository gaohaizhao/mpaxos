/*
 * proposer.h
 *
 *  Created on: Jan 2, 2013
 *      Author: ms
 */

#ifndef PROPOSER_H_
#define PROPOSER_H_

#include <apr_hash.h>
#include <pthread.h>

#include "mpaxos/mpaxos-types.h"
#include "internal_types.h"
#include "utils/safe_assert.h"


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
    roundid_t *rid_ptr;
    apr_pool_t *round_pool;
    apr_hash_t *group_info_ht;  //groupid_t -> group_info_t
    pthread_cond_t prep_cond;
    pthread_cond_t accp_cond;
    pthread_mutex_t mutex;

} round_info_t;


void proposer_init();

void proposer_final();


void handle_msg_promise(Mpaxos__MsgPromise *);

void handle_msg_accepted(Mpaxos__MsgAccepted *);

int run_full_round(groupid_t gid,
        ballotid_t bid,
        Mpaxos__RoundidT **rids_ptr,
        size_t rids_len,
        uint8_t* val,
        size_t val_len,
        uint32_t timeout);

round_info_t* attach_round_info(
        groupid_t gid, roundid_t **rids_ptr, size_t rids_len);

void detach_round_info(
    round_info_t *round_info_ptr);

round_info_t * get_round_info(roundid_t *remote_uurid);

void broadcast_msg_prepare(groupid_t gid,
    roundid_t **rids_ptr, size_t rids_size);

void broadcast_msg_accept(groupid_t gid,
    round_info_t* round_info_ptr,
    Mpaxos__Proposal *prop_p);

#endif /* PROPOSER_H_ */
