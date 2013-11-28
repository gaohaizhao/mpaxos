/*
 * acceptor.cc 
 *
 *  Created on: Nov 9, 2012
 *      Author: frog
 *  ->acceptor.c
 *      Author: msmummy
 */

#include "include_all.h"

static apr_pool_t *mp_accp_ = NULL;
static apr_thread_mutex_t *mx_accp_ = NULL;

static apr_hash_t *ht_accp_info_;

void acceptor_init() {
    apr_pool_create(&mp_accp_, NULL);
    apr_thread_mutex_create(&mx_accp_, APR_THREAD_MUTEX_UNNESTED, mp_accp_);
    
    ht_accp_info_ = apr_hash_make(mp_accp_);
    LOG_INFO("acceptor created");
}

void acceptor_destroy() {
    if (mp_accp_ == NULL) return;
    apr_thread_mutex_destroy(mx_accp_);
	apr_pool_destroy(mp_accp_);
    LOG_INFO("acceptor destroyed");
}

void accp_info_create(accp_info_t **a, instid_t iid) {
    *a = (accp_info_t *)malloc(sizeof(accp_info_t));
    accp_info_t *ainfo = *a;
    ainfo->iid = iid;
    ainfo->bid_max = 0;
    apr_pool_create(&ainfo->mp, NULL);
    ainfo->arr_prop = apr_array_make(ainfo->mp, 0, sizeof(proposal_t *));
    apr_hash_set(ht_accp_info_, &ainfo->iid, sizeof(instid_t), ainfo);
    apr_thread_mutex_create(&ainfo->mx, APR_THREAD_MUTEX_UNNESTED, ainfo->mp);
}

void accp_info_destroy(accp_info_t *ainfo) {
    apr_thread_mutex_destroy(ainfo->mx);
    apr_pool_destroy(ainfo->mp);
    free(ainfo);
}

accp_info_t* get_accp_info(groupid_t gid, slotid_t sid) {
    apr_thread_mutex_lock(mx_accp_);
    instid_t iid;
    memset(&iid, 0, sizeof(iid));
    iid.gid = gid;
    iid.sid = sid;
    accp_info_t *ainfo = NULL;
    size_t sz;
    ainfo = apr_hash_get(ht_accp_info_, &iid, sizeof(instid_t));
    if (ainfo == NULL) {
        // FIXME make sure this is not a value that is decided and forgotten.
        accp_info_create(&ainfo, iid);
    }
    apr_thread_mutex_unlock(mx_accp_);
    return ainfo;
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
    msg_prom.h = &msg_header;
    msg_prom.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__PROMISE;
    msg_prom.h->tid = p_msg_prep->h->tid;
    msg_prom.h->nid = get_local_nid();
  
    msg_prom.n_ress = 0;
    msg_prom.ress = (response_t **)
          calloc(p_msg_prep->n_rids, sizeof(response_t *));
  
    for (int i = 0; i < p_msg_prep->n_rids; i++) {
        roundid_t *rid = p_msg_prep->rids[i];
        if (!is_in_group(rid->gid)) {
            //Check for every requested group in the prepare message.
            //Skip for the groups which I don't belong to.
            continue;
        }
  
        // For those I belong to, add response.
        response_t *p_res = (response_t *)
              calloc(sizeof(response_t), 1);
        msg_prom.ress[msg_prom.n_ress] = p_res;
        mpaxos__response_t__init(p_res);
        p_res->rid = rid;
  
        accp_info_t* ainfo = get_accp_info(rid->gid, rid->sid);
        SAFE_ASSERT(ainfo != NULL);

        // lock on this accp info
        apr_thread_mutex_lock(ainfo->mx);

        // Check if the ballot id is larger.
        // must be greater than. or must be prepared before by the same proposer (TODO)
        if (rid->bid > ainfo->bid_max) {
            p_res->ack = MPAXOS__ACK_ENUM__SUCCESS;
            ainfo->bid_max = rid->bid;
            LOG_DEBUG("prepare is ok. bid: %"PRIx64
                ", seen max bid: %"PRIx64, rid->bid, ainfo->bid_max); 
        } else if (rid->bid < ainfo->bid_max) {
            p_res->ack = MPAXOS__ACK_ENUM__ERR_BID;
            LOG_DEBUG("prepare is not ok. bid: %"PRIx64
                ", seen max bid: %"PRIx64, rid->bid, ainfo->bid_max);
        } else {
            SAFE_ASSERT(0);
        }
  
        // Check if already accepted any proposals.
        // Add them to the response as well.
        apr_array_header_t *arr_prop = ainfo->arr_prop;
        p_res->n_props = arr_prop->nelts;
        LOG_DEBUG("there is %d proposals i have aready got.", arr_prop->nelts);
        p_res->props = (proposal_t**)
              malloc(p_res->n_props * sizeof(proposal_t *));
        for (int i = 0; i < p_res->n_props; i++) {
            proposal_t *oldp = ((proposal_t **)arr_prop->elts)[i];
            p_res->props[i] = oldp;
        }
        msg_prom.n_ress++;
        apr_thread_mutex_unlock(ainfo->mx);
    }
    // Send back the promise message
    size_t sz_msg = mpaxos__msg_promise__get_packed_size(&msg_prom);
    log_message_res("send", "PROMISE", msg_prom.h, msg_prom.ress, 
        msg_prom.n_ress, sz_msg);
    uint8_t *buf = (uint8_t *) malloc(sz_msg);
    mpaxos__msg_promise__pack(&msg_prom, buf);
  
    *rbuf = buf;
    *sz_rbuf = sz_msg;
    
    for (int i = 0; i < msg_prom.n_ress; i++) {
        free(msg_prom.ress[i]->props);
        free(msg_prom.ress[i]);
    }
    free(msg_prom.ress);
}

void handle_msg_accept(const msg_accept_t *msg_accp_ptr, 
    uint8_t** rbuf, size_t *sz_rbuf) {
    SAFE_ASSERT(msg_accp_ptr->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPT);

    // This is the msg_accepted for response
    msg_accepted_t msg_accd = MPAXOS__MSG_ACCEPTED__INIT;
    msg_header_t msg_header = MPAXOS__MSG_HEADER__INIT;
    msg_accd.h = &msg_header;
    msg_accd.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPTED;
    msg_accd.h->tid = msg_accp_ptr->h->tid;
    msg_accd.h->nid = get_local_nid();

    msg_accd.n_ress = 0;
    msg_accd.ress = (response_t **)
            malloc(msg_accp_ptr->prop->n_rids * sizeof(response_t *));

    for (int i = 0; i < msg_accp_ptr->prop->n_rids; i++) {
        roundid_t *rid = msg_accp_ptr->prop->rids[i];
        if (!is_in_group(rid->gid)) {
            continue;
        }

        // For those I belong to, add resoponse.
        response_t *response = (response_t *)
                    malloc(sizeof(response_t));
        msg_accd.ress[msg_accd.n_ress] = response;
        mpaxos__response_t__init(response);
        response->rid = rid;
        
        accp_info_t *ainfo = get_accp_info(rid->gid, rid->sid);
        SAFE_ASSERT(ainfo != NULL);
        
        apr_thread_mutex_lock(ainfo->mx);
        
        // Check if the ballot id is larger.
        ballotid_t maxbid = ainfo->bid_max;
        if (rid->bid >= maxbid) {
            response->ack = MPAXOS__ACK_ENUM__SUCCESS;
            proposal_t **p = apr_array_push(ainfo->arr_prop);
            *p = apr_pcalloc(ainfo->mp, sizeof(proposal_t));
            prop_cpy(*p, msg_accp_ptr->prop, ainfo->mp);
        } else if (rid->bid < maxbid) {
            response->ack = MPAXOS__ACK_ENUM__ERR_BID;
        } else {
            SAFE_ASSERT(0);
        }

        //FIXME whether or not to add proposals in the message.
        //now not
        msg_accd.n_ress++;
        
        apr_thread_mutex_unlock(ainfo->mx);
    }

    // send back the msg_accepted.
    size_t len = mpaxos__msg_accepted__get_packed_size(&msg_accd);
    log_message_res("send", "ACCEPTED", msg_accd.h, msg_accd.ress, 
        msg_accd.n_ress, len);
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
