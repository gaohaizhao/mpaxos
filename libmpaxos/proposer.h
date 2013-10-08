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

void proposer_init();

void proposer_destroy();


void handle_msg_promise(Mpaxos__MsgPromise *);

void handle_msg_accepted(Mpaxos__MsgAccepted *);

int start_round_async(mpaxos_req_t *req);

int phase_1_async_after(round_info_t *rinfo);

int phase_2_async_after(round_info_t *rinfo);

round_info_t *attach_round_info_async(roundid_t **rids, size_t sz_rids, mpaxos_req_t *req);

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

int phase_1_async(round_info_t *rinfo);

int phase_2_async(round_info_t* round_info);

void broadcast_msg_decide(round_info_t *rinfo);

#endif /* PROPOSER_H_ */
