/*
 * proposer.c
 *
 *  Created on: Jan 2, 2013
 *      Author: msmummy
 */

#include "include_all.h"


//static apr_pool_t *mp_prop_ = NULL;

void proposer_init() {
    // initialize the hash table
//    apr_pool_create(&mp_prop_, NULL);

    LOG_INFO("proposer created");
}

void proposer_destroy() {
    LOG_INFO("proposer destroyed");
//    apr_pool_destroy(mp_prop_);
}

/**
 * There could be a few cases for check_promise:
 * 0. majority returns ok.
 * 1. majority returns deny.
 * 2. not majority
 * 
 * @param rinfo
 * @param check_promise
 * @param check_accepted
 * @return 
 */
int check_majority(txn_info_t *tinfo, int phase) {
    SAFE_ASSERT(phase == 1 || phase == 2);
    SAFE_ASSERT(tinfo->ht_ginfo != NULL);
    // check majority for each group, and choose what we will propose
    apr_hash_index_t *hi = NULL;
    int ret = MAJORITY_YES;
    for (hi = apr_hash_first(tinfo->mp, tinfo->ht_ginfo); hi; hi = apr_hash_next(hi)) {
        group_info_t *ginfo = NULL;
        groupid_t *gid = NULL;
        apr_hash_this(hi, (const void**)&gid, NULL, (void **)&ginfo);
        // FIXME group size may change.
        int gsz = get_group_size(*gid);
        if (phase == 1) {
            if (ginfo->n_promises_no >= ((gsz >> 1) + 1)) {
                ret = MAJORITY_NO;
                break;
            } else if (ginfo->n_promises_yes < ((gsz >> 1) + 1)) {
//              LOG_DEBUG(": no majority for group ", gid);
                ret = MAJORITY_UNCERTAIN;
                break;
            }
        } else if (phase == 2) {
            if (ginfo->n_accepteds_no >= ((gsz >> 1) + 1)) {
                ret = MAJORITY_NO;
                break;
            } else if (ginfo->n_accepteds_yes < ((gsz >> 1) + 1)) {
//              LOG_DEBUG(": no majority for group ", *gid_ptr);
                ret = MAJORITY_UNCERTAIN;
                break;
            } else {
                ret = MAJORITY_YES;
            }
        }
    }

    LOG_DEBUG("checking majority %d", ret);
    return ret;
}

//int check_majority_ex(round_info_t *round_info,
//        bool check_promise, bool check_accepted) {
//    apr_thread_mutex_lock(round_info->mx);
//    int ret = check_majority(round_info, check_promise, check_accepted);
//    apr_thread_mutex_unlock(round_info->mx);
//    return ret;
//}

/**
 * The logic of handle promise(ack_prepare)
 * 1. wait until get a majority of response. increment the counters.
 *      yes and no counters
 *      record the max proposal.
 * 2. check if got majority of ok
 *      if yes, go to phase 2;
 *      if no, in the majority of responses, check if got accepted value return;
 *          if yes, check if majority agrees on it.
 *              if yes, decide on that value. tell controller to repropose.
 *              if no, pick one with largest ballot, tell controller.
 *          if no, tell controller to restart phase 1 to prepare again, livelock.
 * @param msg_prom
 */
void handle_msg_promise(msg_promise_t *msg_prom) {
    SAFE_ASSERT(msg_prom->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__PROMISE);
    SAFE_ASSERT(msg_prom->n_ress > 0);

    txnid_t tid = msg_prom->h->tid;
    nodeid_t nid = msg_prom->h->nid;
    txn_info_t *tinfo = get_txn_info(tid);
    if (tinfo == NULL) {
        // this txn is already properly handled and forgotten.
        LOG_DEBUG("no such round for txn: %lu, message too old", tid);
        return;
    }
    // handle this round info.
    apr_thread_mutex_lock(tinfo->mx);
    if (tinfo->in_phase != 1) {
        return;
    }
    
    SAFE_ASSERT(msg_prom->n_ress > 0);
    for (int i = 0; i < msg_prom->n_ress; i++) {
        response_t *response = msg_prom->ress[i];
        groupid_t gid = response->rid->gid;
        slotid_t sid = response->rid->sid;
        ballotid_t bid = response->rid->bid;

        // find the info of this group
        group_info_t *ginfo = NULL;
        ginfo = apr_hash_get(tinfo->ht_ginfo,
                &gid, sizeof(groupid_t));

        // do a not-null check
        SAFE_ASSERT(ginfo != NULL);
        
        // if this is a previous message, ignore it.
        if (gid != ginfo->gid 
                || sid != ginfo->sid 
                || bid != ginfo->bid) {
            LOG_DEBUG("this message contains a outdate response. in message: "
                "gid: %x sid: %"PRIx64" bid: %"PRIx64", in group info: "
                "gid: %x sid: %"PRIx64" bid: %"PRIx64, gid, sid, bid,
                ginfo->gid, ginfo->sid, ginfo->bid);
            continue;
        }

        // adjust number of promises
        ack_enum *re = NULL;
        size_t sz = 0;
        mpr_hash_get(ginfo->ht_prom, &nid, sizeof(nodeid_t), (void **)&re, &sz);
        // for now do not support duplicate messages.
        SAFE_ASSERT(re == NULL);

        if (response->ack == MPAXOS__ACK_ENUM__SUCCESS) {
            LOG_DEBUG("received a yes for prepare.");
            ginfo->n_promises_yes++;
        } else if (response->ack == MPAXOS__ACK_ENUM__ERR_BID) {
            LOG_DEBUG("received a no for prepare");
            // adjust max ballot id proposal
            ginfo->n_promises_no++;
        } else {
            SAFE_ASSERT(0);
        }
        mpr_hash_set(ginfo->ht_prom, &nid, sizeof(nodeid_t), 
                &response->ack, sizeof(ack_enum));
        
        if (response->n_props > 0) {
            //Already accepted some proposals
            for (int j = 0; j < response->n_props; j++) {
                if (ginfo->max_prop == NULL ||
                        ginfo->max_prop->rids[0]->bid
                        < response->props[j]->rids[0]->bid) {

                    if (ginfo->max_prop != NULL) {
                        prop_destroy(ginfo->max_prop);
                    }
                    ginfo->max_prop = malloc(sizeof(proposal_t));
                    prop_cpy(ginfo->max_prop, response->props[j], tinfo->mp);
                    // [IMPORTANT] assume that they have the same rids.
                    
                    if (tinfo->prop_max == NULL 
                            || ginfo->max_prop->rids[0]->bid 
                            > tinfo->prop_max->rids[0]->bid) {
                        tinfo->prop_max = ginfo->max_prop;
                    }
                }
            }
        }
    }
    
    int ret_majority = MAJORITY_UNCERTAIN;
    ret_majority = check_majority(tinfo, 1);
    if (ret_majority == MAJORITY_YES) {
        tinfo->in_phase = 2;
    } else if (ret_majority == MAJORITY_NO) {
        tinfo->in_phase = -1;
    } else if (ret_majority == MAJORITY_UNCERTAIN) {
        // do nothing.
    } else {
        SAFE_ASSERT(0);
    }
    apr_thread_mutex_unlock(tinfo->mx);

    if (ret_majority == MAJORITY_YES) {
        LOG_DEBUG("recevie majority yes for prepare, ready go into phase2.");
        mpaxos_accept(tinfo);
    } else if (ret_majority == MAJORITY_NO) {
        // something is wrong. either:
        // 1. there are some live locks, and we should retry.
        // 2. this slot is already decided.
        LOG_DEBUG("meet a majority no in phase 1.");
        // FIXME Tell the controller to do something.
    } else if (ret_majority == MAJORITY_UNCERTAIN) {
        LOG_DEBUG("receive promise, but not ready to do anything.");
    } else {
        SAFE_ASSERT(0);
    } 
}

/**
 * The logic of handle promise(ack_prepare)
 * 1. wait until get a majority of response. increment the counters.
 *      yes and no counters
 *      record the max proposal.
 * 2. check if got majority of ok
 *      if yes, go to phase 3;
 *      if no, in the majority of responses, check if got accepted value return;
 *          if yes, check if majority agrees on it.
 *              if yes, decide on that value. tell controller to repropose.
 *              if no, pick one with largest ballot, tell controller.
 *          if no, tell controller to restart phase 1 to prepare again, livelock.
 * @param msg_prom
 */
void handle_msg_accepted(msg_accepted_t *msg) {
	SAFE_ASSERT(msg->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPTED);
	SAFE_ASSERT(msg->n_ress > 0);
    
    txnid_t tid = msg->h->tid;
    nodeid_t nid = msg->h->nid;
    txn_info_t *tinfo = get_txn_info(tid);
    if (tinfo == NULL) {
        // this txn is already properly handled and forgotten.
        LOG_DEBUG("no such round for txn: %lu, message too old", tid);
        return;
    }
    // handle this round info.
    apr_thread_mutex_lock(tinfo->mx);
    if (tinfo->in_phase != 2) { //#
        return;
    }

    for (int i = 0; i < msg->n_ress; i++) {
    	response_t *res_ptr = msg->ress[i];
        groupid_t gid = res_ptr->rid->gid;
        slotid_t sid = res_ptr->rid->sid;
        ballotid_t bid = res_ptr->rid->bid;
        
        // find the info of this group
        group_info_t *ginfo = NULL;
        ginfo = apr_hash_get(tinfo->ht_ginfo,
                &gid, sizeof(groupid_t));

        // do a not-null check
        SAFE_ASSERT(ginfo != NULL);

        // if this is a previous message, ignore it.
        if (gid != ginfo->gid 
                || sid != ginfo->sid 
                || bid != ginfo->bid) {
            continue;
        }

        // adjust number of promises
        ack_enum *re = NULL;
        size_t sz = 0;
        mpr_hash_get(ginfo->ht_accd, &nid, sizeof(nodeid_t), (void**)&re, &sz);
        // for now do not support duplicate messages.
        SAFE_ASSERT(re == NULL);

        if (res_ptr->ack == MPAXOS__ACK_ENUM__SUCCESS) {
            LOG_DEBUG("received a yes for prepare.");
            ginfo->n_accepteds_yes ++;
        } else if (res_ptr->ack == MPAXOS__ACK_ENUM__ERR_BID) {
            LOG_DEBUG("received a no for prepare");
            // adjust max ballot id proposal
            ginfo->n_accepteds_no ++;
        } else {
            SAFE_ASSERT(0);
        }
        mpr_hash_set(ginfo->ht_accd, &nid, sizeof(nodeid_t), 
                &res_ptr->ack, sizeof(ack_enum));

        if (res_ptr->n_props > 0) {
            //Already accepted some proposals
            for (int j = 0; j < res_ptr->n_props; j++) {
                if (ginfo->max_prop == NULL ||
                        ginfo->max_prop->rids[0]->bid
                        < res_ptr->props[j]->rids[0]->bid) {

                    if (ginfo->max_prop != NULL) {
                        prop_destroy(ginfo->max_prop);
                    }
                    ginfo->max_prop = malloc(sizeof(proposal_t));
                    prop_cpy(ginfo->max_prop, res_ptr->props[j], tinfo->mp);
                    // [IMPORTANT] assume that they have the same rids.
                    
                    if (tinfo->prop_max == NULL 
                            || ginfo->max_prop->rids[0]->bid 
                            > tinfo->prop_max->rids[0]->bid) {
                        tinfo->prop_max = ginfo->max_prop;
                    }
                }
            }
        }
    }
    
    int ret_majority = MAJORITY_UNCERTAIN;
    ret_majority = check_majority(tinfo, 2); // #
    if (ret_majority == MAJORITY_YES) {
        tinfo->in_phase = 3; // #
    } else if (ret_majority == MAJORITY_NO) {
        tinfo->in_phase = -1;
    } else if (ret_majority == MAJORITY_UNCERTAIN) {
        // do nothing.
    } else {
        SAFE_ASSERT(0);
    }
    apr_thread_mutex_unlock(tinfo->mx);
    
    if (ret_majority == MAJORITY_YES) {
        LOG_DEBUG("recevie majority yes for accept, ready go into phase3."); // #
        phase_2_async_after(tinfo); // #
    } else if (ret_majority == MAJORITY_NO) {
        // something is wrong. either:
        // 1. there are some live locks, and we should retry.
        // 2. this slot is already decided.
        LOG_DEBUG("meet a majority no in phase 2."); // #
        // FIXME Tell the controller to do something.
    } else if (ret_majority == MAJORITY_UNCERTAIN) {
        LOG_DEBUG("receive promise, but not ready to do anything.");
    } else {
        SAFE_ASSERT(0);
    } 
}

int mpaxos_prepare(txn_info_t *info) {
    /*------------------------------- phase I ---------------------------------*/
    // The lock must be before broadcasting, otherwise the cond signal for 
    // pthread may arrive before it actually goes to sleep.
    apr_thread_mutex_lock(info->mx);

    SAFE_ASSERT(info->sz_rids > 0);

    groupid_t gid = info->rids[0]->gid;
    broadcast_msg_prepare(info);
    /*------------------------------- phase I end -----------------------------*/
    apr_thread_mutex_unlock(info->mx);
    return 0;
}

int mpaxos_accept(txn_info_t *tinfo) {
    // check majority for each group, and choose what we will propose
    apr_thread_mutex_lock(tinfo->mx);
    
    SAFE_ASSERT(tinfo->in_phase = 2);
    
	/*------------------------------- phase II start --------------------------*/
    LOG_DEBUG("start phase 2 accept.");
 
    if (tinfo->prop_max != NULL) {
        // for now it should be null.
        proposal_t prop = MPAXOS__PROPOSAL__INIT;
        prop.n_rids = tinfo->prop_max->n_rids;
        prop.rids = malloc(prop.n_rids * sizeof (roundid_t **));
        for (int i = 0; i< prop.n_rids; i++) {
            roundid_t *r = malloc(sizeof(roundid_t));
            mpaxos__roundid_t__init(r);
            r->gid = tinfo->prop_max->rids[i]->gid;
            r->sid = tinfo->prop_max->rids[i]->sid;
            r->bid = tinfo->req->n_retry + 1;
            prop.rids[i] = r;
        }
        prop.value = tinfo->prop_max->value;
        prop.nid = get_local_nid();
        broadcast_msg_accept(tinfo, &prop);
    } else {
        proposal_t *prop_self = apr_palloc(tinfo->mp, sizeof(proposal_t));
        tinfo->prop_self = prop_self;
        mpaxos__proposal__init(prop_self);
        prop_self->n_rids = tinfo->sz_rids;
        prop_self->rids = tinfo->rids;
        prop_self->value.data = tinfo->req->data;
        prop_self->value.len = tinfo->req->sz_data;
        prop_self->nid = get_local_nid();
        broadcast_msg_accept(tinfo, prop_self);
    }
    
	/*----------------------------- phase II end ----------------------------*/
	apr_thread_mutex_unlock(tinfo->mx);
	return 0;
}


int phase_2_async_after(txn_info_t *tinfo) {
    /*-------------------------- learn and decide ---------------------------*/
    apr_thread_mutex_lock(tinfo->mx);

    LOG_DEBUG("all done. decide a value.");

    tinfo->req->sids = malloc(tinfo->sz_rids * sizeof(slotid_t));
    //  remember the decided value.
/*
    for (int i = 0; i < rinfo->sz_rids; i++) {
        roundid_t *rid = rinfo->rids[i];
        groupid_t gid = rid->gid;
        slotid_t sid = rid->sid;
        uint8_t *data = rinfo->req->data;
        size_t sz_data = rinfo->req->sz_data;
        put_instval(gid, sid, data, sz_data);
        rinfo->req->sids[i] = rid->sid;
    }
*/
    proposal_t *prop = (tinfo->prop_max != NULL) ? tinfo->prop_max : tinfo->prop_self;
    SAFE_ASSERT(prop != NULL);
    record_proposal(prop);
    
    // send LEARNED to everybody.
    broadcast_msg_decide(tinfo);
    
    mpaxos_req_t *req = tinfo->req;

    apr_thread_mutex_unlock(tinfo->mx);
    // FIXME notify the controller to detach.
    detach_txn_info(tinfo);

    // go to call back
    async_ready_callback(req);
    return 0;
}


void broadcast_msg_prepare(txn_info_t* tinfo) {

    msg_prepare_t msg_prep = MPAXOS__MSG_PREPARE__INIT;
    msg_header_t header = MPAXOS__MSG_HEADER__INIT;
    size_t sz_rids = tinfo->sz_rids;
    
    msg_prep.h = &header;
    msg_prep.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__PREPARE;
    msg_prep.h->nid = get_local_nid();
    msg_prep.h->tid = tinfo->tid;
    msg_prep.n_rids = sz_rids;
    msg_prep.rids = tinfo->rids;

    // send the message
    groupid_t* gids = (groupid_t*)malloc(sz_rids * sizeof(groupid_t));
    for (int i = 0; i < sz_rids; i++) {
        gids[i] = tinfo->rids[i]->gid;
    }

    // This is calculated packing length
    size_t sz_msg = mpaxos__msg_prepare__get_packed_size (&msg_prep);
    uint8_t *buf = (uint8_t *)malloc(sz_msg);
    mpaxos__msg_prepare__pack(&msg_prep, buf);
    log_message_rid("broadcast", "PREPARE", msg_prep.h, msg_prep.rids, msg_prep.n_rids, sz_msg);
    send_to_groups(gids, sz_rids, MSG_PREPARE, (const char *)buf, sz_msg);

    free(buf);
    free(gids);
}

//TODO check, feel something wrong.
void broadcast_msg_accept(txn_info_t *tinfo,
    proposal_t *prop_p) {
    msg_accept_t msg_accp = MPAXOS__MSG_ACCEPT__INIT;
    msg_header_t header = MPAXOS__MSG_HEADER__INIT;
    msg_accp.h = &header;
    msg_accp.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPT;
    msg_accp.h->tid = tinfo->tid;
    msg_accp.h->nid = get_local_nid();
    msg_accp.prop = prop_p;

//  apr_hash_index_t *hi = NULL;
//  for (hi = apr_hash_first(pool_ptr, round_info_ptr->group_info_ht); hi; hi = apr_hash_next(hi)) {
//      groupid_t *gid_ptr;
//      group_info_t *ginfo_ptr;
//      apr_hash_this(hi, (const void**)&gid_ptr, NULL, (void**)&ginfo_ptr);
//      SAFE_ASSERT(gid_ptr != NULL && ginfo_ptr != NULL);
//  }

    groupid_t* gids = (groupid_t*)malloc(prop_p->n_rids * sizeof(groupid_t));
    for (int i = 0; i < prop_p->n_rids; i++) {
        gids[i] = prop_p->rids[i]->gid;
    }

    size_t sz_msg = mpaxos__msg_accept__get_packed_size (&msg_accp);
    log_message_rid("broadcast", "ACCEPT", msg_accp.h, prop_p->rids, prop_p->n_rids, sz_msg);
    char *buf = (char *)malloc(sz_msg);
    mpaxos__msg_accept__pack(&msg_accp, (uint8_t *)buf);
    send_to_groups(gids, prop_p->n_rids, MSG_ACCEPT, buf, sz_msg);

    free(buf);
    free(gids);
}


//TODO check, many things are wrong.
void broadcast_msg_decide(txn_info_t *rinfo) {
    proposal_t prop = MPAXOS__PROPOSAL__INIT;
/*
    if (rinfo->prop_max != NULL) {
        rinfo->is_voriginal = -1;
        prop.n_rids = rinfo->prop_max->n_rids;
        prop.rids = malloc(prop.n_rids * sizeof (roundid_t **));
        for (int i = 0; i< prop.n_rids; i++) {
            roundid_t *r = malloc(sizeof(roundid_t));
            mpaxos__roundid_t__init(r);
            r->gid = rinfo->prop_max->rids[i]->gid;
            r->sid = rinfo->prop_max->rids[i]->sid;
            r->bid = rinfo->req->n_retry + 1;
            prop.rids[i] = r;
        }
        prop.value = rinfo->prop_max->value;
    } else {
        prop.n_rids = rinfo->sz_rids;
        prop.rids = rinfo->rids;
        prop.value.data = rinfo->req->data;
        prop.value.len = rinfo->req->sz_data;
    }
*/
    proposal_t *prop_src = rinfo->prop_max != NULL ? rinfo->prop_max : rinfo->prop_self;
    SAFE_ASSERT(prop_src != NULL);
    prop_cpy(&prop, prop_src, rinfo->mp);
    prop.value.len = 0;
    prop.value.data = NULL;
    
    msg_decide_t msg_dcd = MPAXOS__MSG_DECIDE__INIT;
    msg_header_t header = MPAXOS__MSG_HEADER__INIT;
    msg_dcd.h = &header;
    msg_dcd.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__DECIDE;
    msg_dcd.h->nid = get_local_nid();
    msg_dcd.prop = &prop;

//  apr_hash_index_t *hi = NULL;
//  for (hi = apr_hash_first(pool_ptr, round_info_ptr->group_info_ht); hi; hi = apr_hash_next(hi)) {
//      groupid_t *gid_ptr;
//      group_info_t *ginfo_ptr;
//      apr_hash_this(hi, (const void**)&gid_ptr, NULL, (void**)&ginfo_ptr);
//      SAFE_ASSERT(gid_ptr != NULL && ginfo_ptr != NULL);
//  }

    groupid_t* gids = (groupid_t*)malloc(prop.n_rids * sizeof(groupid_t));
    for (int i = 0; i < prop.n_rids; i++) {
        gids[i] = prop.rids[i]->gid;
    }

    size_t sz_msg = mpaxos__msg_decide__get_packed_size (&msg_dcd);
    log_message_rid("broadcast", "DECIDE", msg_dcd.h, prop.rids, prop.n_rids, sz_msg);
    char *buf = (char *)malloc(sz_msg);
    mpaxos__msg_decide__pack(&msg_dcd, (uint8_t *)buf);
    send_to_groups(gids, prop.n_rids, MSG_DECIDE, buf, sz_msg);

    free(buf);
    free(gids);
}
