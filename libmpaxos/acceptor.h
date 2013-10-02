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
