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

int phase_2_async_after(txn_info_t *rinfo);

void broadcast_msg_prepare(txn_info_t *tinfo);

void broadcast_msg_accept(txn_info_t* tinfo,
		Mpaxos__Proposal *prop_p);

int mpaxos_prepare(txn_info_t *tinfo);

int mpaxos_accept(txn_info_t *tinfo);

void broadcast_msg_decide(txn_info_t *rinfo);

#endif /* PROPOSER_H_ */
