/*
 * acceptor.h
 *
 *  Created on: Jan 5, 2013
 *      Author: ms
 */

#ifndef ACCEPTOR_H_
#define ACCEPTOR_H_

#include <apr_tables.h>
#include "mpaxos/mpaxos-types.h"
#include "internal_types.h"

typedef struct {
    apr_pool_t *mp;
    instid_t iid;
    ballotid_t bid_max;
    apr_array_header_t *arr_prop;
    apr_thread_mutex_t *mx;
} accp_info_t;

void acceptor_init();
void acceptor_destroy();

void acceptor_forget();

void handle_msg_prepare(const Mpaxos__MsgPrepare *, uint8_t** rbuf, size_t* sz_rbuf);
void handle_msg_accept(const Mpaxos__MsgAccept *, uint8_t** rbuf, size_t* sz_rbuf);
//void handle_msg_learn(const mpaxos::msg_learn *);

void get_inst_bid(groupid_t gid, slotid_t sids,
      ballotid_t*);

void put_inst_bid(groupid_t gid, slotid_t sids,
      ballotid_t);

void put_inst_prop(groupid_t gid, slotid_t sid,
      const Mpaxos__Proposal *prop);

apr_array_header_t *get_inst_prop_vec(groupid_t gid, slotid_t sid);

#endif /* ACCEPTOR_H_ */
