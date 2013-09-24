/*
 * acceptor.cc
 *
 *  Created on: Nov 9, 2012
 *      Author: frog
 */

#include <stdio.h>
#include <stdlib.h>
#include <apr_hash.h>
#include <apr_tables.h>

#include "mpaxos/mpaxos.h"
#include "utils/logger.h"
#include "utils/safe_assert.h"
#include "acceptor.h"
#include "comm.h"
#include "log_helper.h"

apr_pool_t *pl_accp_;
apr_hash_t *ht_prom_; // instid_t -> ballotid_t
apr_hash_t *ht_accp_; // instid_t -> array<proposal>


void acceptor_init() {
    apr_pool_create(&pl_accp_, NULL);
    ht_prom_ = apr_hash_make(pl_accp_);
    ht_accp_ = apr_hash_make(pl_accp_);

  LOG_INFO("acceptor created");
}

void acceptor_final() {
    if (pl_accp_ == NULL) return;
	apr_pool_destroy(pl_accp_);
    LOG_INFO("acceptor destroyed");
}

void acceptor_forget() {
    // TODO clean the promise and accept map.
    // This is only temporary for debug 
    //apr_pool_clear(accept_pool);
    //promise_ht_ = apr_hash_make(accept_pool);
    //accept_ht_ = apr_hash_make(accept_pool);
}

void handle_msg_prepare(const msg_prepare_t *p_msg_prep, uint8_t** rbuf, size_t* sz_rbuf) {
  SAFE_ASSERT(p_msg_prep->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__PREPARE);

  // This is the msg_promise for response
  msg_promise_t msg_prom = MPAXOS__MSG_PROMISE__INIT;
  msg_header_t msg_header = MPAXOS__MSG_HEADER__INIT;
  processid_t pid = MPAXOS__PROCESSID_T__INIT;
  msg_prom.h = &msg_header;
  msg_prom.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__PROMISE;
  msg_prom.h->pid = &pid;

  // TODO set to zero might not be a good idea.
  msg_prom.h->pid->gid = 0;
  msg_prom.h->pid->nid = get_local_nid();

  msg_prom.n_ress = 0;
  msg_prom.ress = (response_t **)
        malloc(p_msg_prep->n_rids * sizeof(response_t *));

  for (int i = 0; i < p_msg_prep->n_rids; i++) {
    roundid_t *p_rid = p_msg_prep->rids[i];
      if (!is_in_group(p_rid->gid)) {
          //Check for every requested group in the prepare message.
          //Skip for the groups which I don't belong to.
          continue;
      }

      // For those I belong to, add response.
      response_t *p_res = (response_t *)
            malloc(sizeof(response_t));
      msg_prom.ress[msg_prom.n_ress] = p_res;
      mpaxos__response_t__init(p_res);
      p_res->rid = p_rid;

      // Check if the ballot id is larger.
      ballotid_t maxbid = 0;
      get_inst_bid(p_rid->gid, p_rid->sid, &maxbid);
      if (p_rid->bid >= maxbid) {
          p_res->ack = MPAXOS__ACK_ENUM__SUCCESS;
          put_inst_bid(p_rid->gid, p_rid->sid, p_rid->bid);
      } else {
        p_res->ack = MPAXOS__ACK_ENUM__ERR_BID;
      }

      // Check if already accepted any proposals.
      // Add them to the response as well.
      apr_array_header_t *prop_arr;
      prop_arr = get_inst_prop_vec(p_rid->gid, p_rid->sid);
      p_res->n_props = prop_arr->nelts;
      p_res->props = (Mpaxos__Proposal**)
            malloc(p_res->n_props * sizeof(Mpaxos__Proposal *));
      for (int i = 0; i < p_res->n_props; i++) {
        p_res->props[i] = (proposal*)&(prop_arr->elts[i]);
      }
      msg_prom.n_ress++;
  }

  // Send back the promise message
  size_t len = mpaxos__msg_promise__get_packed_size(&msg_prom);
  log_message_res("send", "PROMISE", msg_prom.h, msg_prom.ress, msg_prom.n_ress, len);
  uint8_t *buf = (uint8_t *) malloc(len);
  mpaxos__msg_promise__pack(&msg_prom, buf);
/*
  send_to(p_msg_prep->h->pid->nid, buf, len);
*/

  *rbuf = buf;
  *sz_rbuf = len;
/*
  free(buf);
*/
  for (int i = 0; i < msg_prom.n_ress; i++) {
        free(msg_prom.ress[i]->props);
    free(msg_prom.ress[i]);
  }
  free(msg_prom.ress);
}

void handle_msg_accept(const msg_accept_t *msg_accp_ptr, uint8_t** rbuf, size_t *sz_rbuf) {
    SAFE_ASSERT(msg_accp_ptr->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPT);

    // This is the msg_accepted for response
  Mpaxos__MsgAccepted msg_accd = MPAXOS__MSG_ACCEPTED__INIT;
  Mpaxos__MsgHeader msg_header = MPAXOS__MSG_HEADER__INIT;
  Mpaxos__ProcessidT pid = MPAXOS__PROCESSID_T__INIT;
  msg_accd.h = &msg_header;
  msg_accd.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPTED;
  msg_accd.h->pid = &pid;

    //TODO set to zero might not be a good idea.
  msg_accd.h->pid->gid = 0;
  msg_accd.h->pid->nid = get_local_nid();

  msg_accd.n_ress = 0;
  msg_accd.ress = (Mpaxos__ResponseT **)
            malloc(msg_accp_ptr->prop->n_rids * sizeof(Mpaxos__ResponseT *));

    for (int i = 0; i < msg_accp_ptr->prop->n_rids; i++) {
        Mpaxos__RoundidT *rid_ptr = msg_accp_ptr->prop->rids[i];
        if (!is_in_group(rid_ptr->gid)) {
            continue;
        }

        // For those I belong to, add resoponse.
        Mpaxos__ResponseT *res_ptr = (Mpaxos__ResponseT *)
                    malloc(sizeof(Mpaxos__ResponseT));
        msg_accd.ress[msg_accd.n_ress] = res_ptr;
        mpaxos__response_t__init(res_ptr);
        res_ptr->rid = rid_ptr;


        // Check if the ballot id is larger.
        ballotid_t maxbid;
        get_inst_bid(rid_ptr->gid, rid_ptr->sid, &maxbid);
        if (rid_ptr->bid >= maxbid) {
          res_ptr->ack = MPAXOS__ACK_ENUM__SUCCESS;
            put_inst_prop(rid_ptr->gid, rid_ptr->sid, msg_accp_ptr->prop);
      } else {
        res_ptr->ack = MPAXOS__ACK_ENUM__ERR_BID;
      }

        //TODO whether or not to add proposals in the message.
        //now not

        msg_accd.n_ress++;
    }

    // send back the msg_accepted.
    size_t len = mpaxos__msg_accepted__get_packed_size(&msg_accd);
    log_message_res("send", "ACCEPTED", msg_accd.h, msg_accd.ress, msg_accd.n_ress, len);
    uint8_t *buf = (uint8_t *) malloc(len);
    mpaxos__msg_accepted__pack(&msg_accd, buf);
/*
  send_to(msg_accp_ptr->h->pid->nid, buf, len);
*/
  *rbuf = buf;
  *sz_rbuf = len;
/*
  free(buf);
*/
  for (int i = 0; i < msg_accd.n_ress; i++) {
    free(msg_accd.ress[i]);
  }
  free(msg_accd.ress);

}

//void handle_msg_learn(const mpaxos::msg_learn &msg) {
    //TODO
//  SAFE_ASSERT(msg.h().t() == mpaxos::msg_header::LEARN);
//
//  msg_accepted *msg_accd_p = new msg_accepted();
//  msg_accd_p->mutable_h()->set_t(mpaxos::msg_header::ACCEPTED);
//  //TODO set to zero might not be a good idea.
//  msg_accd_p->mutable_h()->mutable_pid()->set_gid(0);
//  msg_accd_p->mutable_h()->mutable_pid()->set_nid(P_PAXOS_->get_nid());
//
//  for (int i = 0; i < msg.rids_size() && i < 1/* TODO */; i++) {
//      roundid_t rid = msg.rids(i);
//      if (!P_PAXOS_->is_ingroup(rid.gid())) {
//          continue;
//      }
//      proposal prop;
//      int r = get_inst_prop(rid.gid(), rid.sid(), &prop);
//      if (r == 0) {
//          msg_accd_p->mutable_prop()->CopyFrom(prop);
//      }
//  }
//  if (msg_accd_p->prop().rids_size() > 0) {
//      std::string s = msg_accd_p->SerializeAsString();
//      send_to(msg.h().pid().nid(), s.data(), s.size());
//  }

//  LOG_DEBUG(to_string(), ": sent ACCEPTED message to group ",
//      msg.h.pid.gid, " node ", msg.h.pid.nid);
//}

void get_inst_bid(groupid_t gid, slotid_t sid,
    ballotid_t *bid) {
    instid_t *iid = calloc(sizeof(instid_t), 1);
    iid->gid = gid;
    iid->sid = sid;
    ballotid_t *r = apr_hash_get(ht_prom_, iid, sizeof(instid_t));
    if (r == NULL) {
        instid_t *i = apr_palloc(pl_accp_, sizeof(instid_t));
        *i = *iid;
        r = apr_palloc(pl_accp_, sizeof(bid));
        *r = 0;
        apr_hash_set(ht_prom_, i, sizeof(instid_t), r);
    }
    *bid = *r;
    free(iid);
}

void put_inst_bid(groupid_t gid, slotid_t sid,
    ballotid_t bid) {
    instid_t *iid_ptr;
    iid_ptr = apr_palloc(pl_accp_, sizeof(instid_t));
    mpaxos__instid_t__init(iid_ptr);
    iid_ptr->gid = gid;
    iid_ptr->sid = sid;

    ballotid_t *r = apr_hash_get(ht_prom_, iid_ptr, sizeof(instid_t));
    if (r == NULL) {
        r = apr_palloc(pl_accp_, sizeof(ballotid_t));
        *r = bid;
        apr_hash_set(ht_prom_, iid_ptr, sizeof(instid_t), r);
    } else {
        *r = bid;
        apr_hash_set(ht_prom_, iid_ptr, sizeof(instid_t), r);
        //free(iid_ptr);
    }
}

/**
 * return
 *  0 success
 *  -1 not found
 */
apr_array_header_t *get_inst_prop_vec(
        groupid_t gid, slotid_t sid) {
    instid_t *iid = calloc(sizeof(instid_t), 1);
    iid->gid = gid;
    iid->sid = sid;

    apr_array_header_t *arr = apr_hash_get(ht_accp_, iid, sizeof(instid_t));
    if (arr == NULL) {
        instid_t *i = apr_palloc(pl_accp_, sizeof(instid_t));
        *i = *iid;
        arr = apr_array_make(pl_accp_, 1, sizeof(proposal));
        apr_hash_set(ht_accp_, i, sizeof(instid_t), arr);
    }
    
    free(iid);
    return arr;
}

void put_inst_prop(groupid_t gid, slotid_t sid,
    const proposal *prop) {
    instid_t *iid = calloc(1, sizeof(instid_t));
    iid->gid = gid;
    iid->sid = sid;

    apr_array_header_t *arr = apr_hash_get(ht_accp_, iid, sizeof(instid_t));
    if (arr == NULL) {
        instid_t *i = apr_palloc(pl_accp_, sizeof(instid_t));
        *i = *iid;
        arr = apr_array_make(pl_accp_, 1, sizeof(proposal));
        apr_hash_set(ht_accp_, i, sizeof(instid_t), arr);
    }
    
    proposal *p = apr_array_push(arr);
    free(iid);
    prop_cpy(p, prop, pl_accp_);
}

//void broadcast_accepted_msg(const std::vector<proposal> &prop_vec) {
//  msg_accepted msg;
//  msg.h.type = MSG_ACCEPTED;
//  msg.h.pid.gid = P_PAXOS_->GID_;
//  msg.h.pid.nid = P_PAXOS_->NID_;
//  msg.prop_vec = prop_vec;
//
//  std::vector<slot> slot_vec = prop_vec[prop_vec.size() - 1].slot_vec;
//  for (size_t i = 0; i < slot_vec.size(); i++) {
//    if (!slot_vec[i].f_involved) {
//      continue;
//    }
//
//    msg.sid = slot_vec[i].sid;
//
//    io::marshal msg_mar;
//    msg_mar << msg;
//    P_PAXOS_->communicator_.send_to_group((uint32_t) i, msg_mar.c_str(),
//        msg_mar.size());
//  }
//}

