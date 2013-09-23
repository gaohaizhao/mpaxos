/*
 * proposer.c
 *
 *  Created on: Jan 2, 2013
 *      Author: msmummy
 */

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <pthread.h>
#include <apr_hash.h>

#include "comm.h"
#include "proposer.h"
#include "mpaxos/mpaxos.h"
#include "utils/logger.h"
#include "utils/mtime.h"
#include "log_helper.h"
#include "view.h"
#include "async.h"
#include "internal_types.h"


apr_hash_t *round_info_ht_; //roundid_t -> round_info

static pthread_mutex_t round_info_mutex_;

static pthread_mutex_t *group_mutexs_;

void lock_group(groupid_t gid) {
    pthread_mutex_lock(group_mutexs_ + gid);
}

void unlock_group(groupid_t gid) {
    pthread_mutex_unlock(group_mutexs_ + gid);
}

void group_info_init(round_info_t *r, group_info_t *g) {
    g->promise_ht = apr_hash_make(r->round_pool);
    g->accepted_ht = apr_hash_make(r->round_pool);
    g->n_promises = 0;
    g->n_accepteds = 0;
    g->max_bid_prop_ptr = NULL;
    pthread_mutex_init(&g->mutex, NULL);
}

void group_info_final(group_info_t *g) {
    if (g->max_bid_prop_ptr != NULL) {
        //TODO destroy
        prop_destroy(g->max_bid_prop_ptr);
        free(g->max_bid_prop_ptr);
    }
}

void round_info_init(round_info_t *r) {
    apr_pool_create(&r->round_pool, NULL);
    apr_thread_mutex_create(&r->mx, APR_THREAD_MUTEX_UNNESTED, r->round_pool);
//    apr_thread_cond_create(&r->cond_accp, r->round_pool);
//    apr_thread_cond_create(&r->cond_prep, r->round_pool);
    r->group_info_ht = apr_hash_make(r->round_pool);
    r->after_phase1 = 0;
    r->after_phase2 = 0;
}

void round_info_final(round_info_t *r) {
    r->rid = NULL;
/*
    SAFE_ASSERT(pthread_mutex_destroy(&r->mutex) == 0);
    SAFE_ASSERT(pthread_cond_destroy(&r->prep_cond) == 0);
    SAFE_ASSERT(pthread_cond_destroy(&r->accp_cond) == 0);
*/
    apr_pool_destroy(r->round_pool);
}

void proposer_init() {
    // initialize the hash table
    //roundid_ht_ = apr_hash_make(pool_ptr);
    round_info_ht_ = apr_hash_make(pl_global_);

    SAFE_ASSERT(pthread_mutex_init(&round_info_mutex_, NULL) == 0);


  //TODO
    int n_groups = 10;
    group_mutexs_ = malloc((n_groups + 1) * sizeof(pthread_mutex_t));
    for (int i = 1; i <= n_groups; i++) {
        pthread_mutex_init(group_mutexs_ + i, NULL);
    }

    LOG_INFO("proposer created");
}

void proposer_final() {
    SAFE_ASSERT(pthread_mutex_destroy(&round_info_mutex_) == 0);
    LOG_INFO("proposer destroyed");
  
    //TODO
    int n_groups = 10;
    for (int i = 1; i <= n_groups; i++) {
      pthread_mutex_destroy(group_mutexs_ + i);
    }
    free(group_mutexs_);
}

bool check_majority(round_info_t *rinfo,
        bool check_promise, bool check_accepted) {
    // check majority for each group, and choose what we will propose

    apr_hash_index_t *hi;
    SAFE_ASSERT(rinfo->group_info_ht != NULL);
    for (hi = apr_hash_first(rinfo->round_pool, rinfo->group_info_ht); hi; hi = apr_hash_next(hi)) {
        roundid_t *rid_ptr;
        group_info_t *ginfo_ptr;
        apr_hash_this(hi, (const void**)&rid_ptr, NULL, (void **)&ginfo_ptr);
        int gsz = get_group_size(rid_ptr->gid);
        if (check_promise) {
            if (ginfo_ptr->n_promises < ((gsz >> 1) + 1)) {
//              LOG_DEBUG(": no majority for group ", gid);
                return false;
            }
        }

        if (check_accepted) {
            if (ginfo_ptr->n_accepteds < ((gsz >> 1) + 1)) {
//              LOG_DEBUG(": no majority for group ", *gid_ptr);
                return false;
            }
        }
    }

    return true;
}


bool check_majority_ex(round_info_t *round_info,
        bool check_promise, bool check_accepted) {
    apr_thread_mutex_lock(round_info->mx);
    bool ret = check_majority(round_info, check_promise, check_accepted);
    apr_thread_mutex_unlock(round_info->mx);
    return ret;
}


void handle_msg_promise(msg_promise_t *msg_prom) {
    SAFE_ASSERT(msg_prom->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__PROMISE);
    SAFE_ASSERT(msg_prom->n_ress > 0);

    pthread_mutex_lock(&round_info_mutex_);

    bool sig = FALSE;
    round_info_t *rinfo;
    for (int i = 0; i < msg_prom->n_ress; i++) {
        response_t *res_ptr = msg_prom->ress[i];
        roundid_t remote_rid;
        memset(&remote_rid, 0, sizeof(remote_rid));
        
        remote_rid.gid = res_ptr->rid->gid;
        remote_rid.sid = res_ptr->rid->sid;
        remote_rid.bid = res_ptr->rid->bid;
        rinfo = get_round_info(&remote_rid);

        if (rinfo == NULL) {
            LOG_DEBUG(": no such round, message too old or too future");
            continue;
        }

        // handle this round info.
        apr_thread_mutex_lock(rinfo->mx);

        // find this group
        group_info_t *group_info_ptr;
        group_info_ptr = apr_hash_get(rinfo->group_info_ht,
                &remote_rid, sizeof(roundid_t));

        // do a not-null check
        SAFE_ASSERT(group_info_ptr != NULL);

        // adjust number of promises
        nodeid_t *nid_ptr = apr_pcalloc(rinfo->round_pool, sizeof(nodeid_t));
        nid_ptr = &msg_prom->h->pid->nid;
        ack_enum *re = apr_hash_get(group_info_ptr->promise_ht, nid_ptr, sizeof(nodeid_t));
        if (re == NULL) {
            re = apr_pcalloc(rinfo->round_pool, sizeof(ack_enum));
            *re = MPAXOS__ACK_ENUM__ERR_BID;
        }

        if (res_ptr->ack == MPAXOS__ACK_ENUM__SUCCESS) {
          if (*re != MPAXOS__ACK_ENUM__SUCCESS) {
            group_info_ptr->n_promises++;
          }
            *re = MPAXOS__ACK_ENUM__SUCCESS;
        } else if (res_ptr->ack == MPAXOS__ACK_ENUM__ERR_BID) {
            // adjust max ballot id proposal
            *re = MPAXOS__ACK_ENUM__ERR_BID;
        }
        apr_hash_set(group_info_ptr->promise_ht, nid_ptr, sizeof(nodeid_t), re);

        if (res_ptr->n_props > 0) {
            //Already accepted some proposals
            for (int j = 0; j < res_ptr->n_props; j++) {
                if (group_info_ptr->max_bid_prop_ptr == NULL ||
                        group_info_ptr->max_bid_prop_ptr->rids[0]->bid
                                                < res_ptr->props[j]->rids[0]->bid) {


                    if (group_info_ptr->max_bid_prop_ptr != NULL) {
                        prop_destroy(group_info_ptr->max_bid_prop_ptr);
                        free(group_info_ptr->max_bid_prop_ptr);
                    }
                    group_info_ptr->max_bid_prop_ptr = malloc(sizeof(proposal));
                    prop_cpy(group_info_ptr->max_bid_prop_ptr, res_ptr->props[j], rinfo->round_pool);
                }
            }
        }
        if (!rinfo->after_phase2) {
            sig = check_majority(rinfo, true, false);
        }

        // check if we can signal this round to stop waiting
        apr_thread_mutex_unlock(rinfo->mx);
    }
    pthread_mutex_unlock(&round_info_mutex_);
    if (sig) {
        LOG_TRACE("after phase1.");
        // apr_status_t status = apr_thread_cond_signal(round_info_ptr->cond_prep);
        // SAFE_ASSERT(status == APR_SUCCESS);
        phase_1_async_after(rinfo);
    } else {
        // is there any dead locks?
        LOG_DEBUG("not ready to go into phase 2.");
    }
}

void handle_msg_accepted(msg_accepted_t *msg) {
	SAFE_ASSERT(msg->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPTED);
	SAFE_ASSERT(msg->n_ress > 0);

    pthread_mutex_lock(&round_info_mutex_);

    bool sig = FALSE;
    round_info_t *rinfo;
    for (int i = 0; i < msg->n_ress; i++) {
    	response_t *res_ptr = msg->ress[i];
    	roundid_t remote_rid;
        memset(&remote_rid, 0, sizeof(roundid_t));
        remote_rid.gid = res_ptr->rid->gid;
        remote_rid.sid = res_ptr->rid->sid;
        remote_rid.bid = res_ptr->rid->bid;
        rinfo = get_round_info(&remote_rid);

        if (rinfo == NULL) {
        	LOG_DEBUG("no such round, message too old or too future");
            continue;
        }

        // handle this round info.
        apr_thread_mutex_lock(rinfo->mx);

        // find this group
        group_info_t *group_info_ptr;
        group_info_ptr = apr_hash_get(rinfo->group_info_ht,
                &remote_rid, sizeof(roundid_t));
        // do a not-null check
        SAFE_ASSERT(group_info_ptr != NULL);


        // adjust number of promises
        nodeid_t *nid_ptr = apr_pcalloc(rinfo->round_pool, sizeof(nodeid_t));
        nid_ptr = &msg->h->pid->nid;
        ack_enum *re = apr_hash_get(group_info_ptr->accepted_ht,
                nid_ptr, sizeof(nodeid_t));
        if (re == NULL ) {
            re = apr_pcalloc(rinfo->round_pool, sizeof(ack_enum));
            *re = MPAXOS__ACK_ENUM__ERR_BID;
        }

        if (res_ptr->ack == MPAXOS__ACK_ENUM__SUCCESS) {
            if (*re != MPAXOS__ACK_ENUM__SUCCESS) {
                group_info_ptr->n_accepteds++;
            }
            *re = MPAXOS__ACK_ENUM__SUCCESS;
        } else if (res_ptr->ack == MPAXOS__ACK_ENUM__ERR_BID) {
            // adjust max ballot id proposal
            *re = MPAXOS__ACK_ENUM__ERR_BID;
        }
        apr_hash_set(group_info_ptr->accepted_ht, nid_ptr, sizeof(nodeid_t), re);

        if (!rinfo->after_phase2) {
            sig = check_majority(rinfo, false, true);
        }
        
        apr_thread_mutex_unlock(rinfo->mx);

        // If I receive an msg_accepted, I must have sent a proposal.
        // I should have remembered the proposal somewhere.
        // TODO bother to check.

        // check if we can signal this round to stop waiting
    }
    pthread_mutex_unlock(&round_info_mutex_);
    if (sig) {
    	LOG_DEBUG("after phase2.");
        // apr_status_t status = apr_thread_cond_signal(round_info_ptr->cond_accp);
        // SAFE_ASSERT(status == APR_SUCCESS);
        phase_2_async_after(rinfo);
    }
}

/**
 * 
 * @param cond
 * @param mutex
 * @param timeout
 * @return 
 */
int sleep_and_wait(apr_thread_cond_t *cond, apr_thread_mutex_t *mutex, uint32_t timeout) {
    // wait
    apr_time_t t_to = timeout * 1000 * 1000;
    LOG_DEBUG("go to sleep");
    apr_status_t status = apr_thread_cond_timedwait(cond, mutex, t_to);
    if (status == APR_SUCCESS) {
        
    } else if (status == APR_TIMEUP) {
        // TODO [improve] tolerate timeout
        SAFE_ASSERT(0);
    } else {
        SAFE_ASSERT(0);
    }
    //sprintf(out, "sleep %d ms", (ts_end.tv_sec - ts_begin.tv_sec) * 1000 + (ts_end.tv_nsec - ts_begin.tv_nsec) / 1000 / 1000);
    //LOG_DEBUG(out);
    return 0;
}

int phase_1_async(round_info_t *rinfo) {
    /*------------------------------- phase I ---------------------------------*/
    //The lock must be before broadcasting, otherwise the cond signal for pthread may arrive before it actually goes to sleep.
    apr_thread_mutex_lock(rinfo->mx);

    SAFE_ASSERT(rinfo->sz_rids > 0);
//  SAFE_ASSERT(round_info_ptr->group_info_map.size() == rids_len);

    groupid_t gid = rinfo->rids[0]->gid;
    broadcast_msg_prepare(gid, rinfo->rids, rinfo->sz_rids);
    /*------------------------------- phase I end -----------------------------*/
    //not wait
    // sleep_and_wait(round_info->cond_prep, round_info->mx, timeout);

    apr_thread_mutex_unlock(rinfo->mx);
    return 0;
}

int phase_2_async(round_info_t* rinfo) {
	/*------------------------------- phase II start --------------------------*/
	apr_thread_mutex_lock(rinfo->mx);

	proposal_t prop = MPAXOS__PROPOSAL__INIT;
	prop.n_rids = rinfo->sz_rids;
	prop.rids = rinfo->rids;
	prop.value.data = rinfo->req->data;
	prop.value.len = rinfo->req->sz_data;
	groupid_t gid = rinfo->rid->gid;
	broadcast_msg_accept(gid, rinfo, &prop);
	/*----------------------------- phase II end ----------------------------*/
	// wait
	// sleep_and_wait(round_info->cond_accp, round_info->mx, timeout);
	apr_thread_mutex_unlock(rinfo->mx);
	return 0;
}

int run_full_round_async(
        roundid_t **rids,
        mpaxos_req_t *req) {
    LOG_DEBUG("Start to run full round.");
    size_t sz_rids = req->sz_gids;

    round_info_t *rinfo = attach_round_info_async(
              rids, sz_rids, req);


    LOG_DEBUG("Start phase 1 prepare.");
    phase_1_async(rinfo);
    return 0;
}

int phase_1_async_after(round_info_t *rinfo) {
	rinfo->after_phase1 = 1;

    // check majority for each group, and choose what we will propose
	bool ret = check_majority_ex(rinfo, true, false);
    SAFE_ASSERT(ret == TRUE);

    LOG_DEBUG("Start phase 2 accept.");
    // TODO [fix]
    phase_2_async(rinfo);
    return 0;
}

int phase_2_async_after(round_info_t *rinfo) {
    /*-------------------------- learn and decide ---------------------------*/
	rinfo->after_phase2 = 1;

    // check majority for each group for accept
    if (!check_majority_ex(rinfo, false, true)) {
            return -2;
    }

    LOG_DEBUG("all done. decide.");

    detach_round_info(rinfo);

    // go to call back
    async_ready_callback(rinfo->req);
    return 0;
}

round_info_t *attach_round_info(
        groupid_t gid, roundid_t **rids_ptr, size_t rids_len) {
    pthread_mutex_lock(&round_info_mutex_);


    //find local_rid from rid_vec
    //roundid_t *local_rid_ptr = calloc(sizeof(roundid_t), 1);
    roundid_t *local_rid_ptr = NULL;
    for (int i = 0; i < rids_len; i++) {
        if (rids_ptr[i]->gid == gid) {
        //local_rid_ptr->gid = rids_ptr[i]->gid;
        //local_rid_ptr->sid = rids_ptr[i]->sid;
        //local_rid_ptr->bid = rids_ptr[i]->bid;
            local_rid_ptr = rids_ptr[i];
            break;
        }
    }

    round_info_t *rinfo_ptr = apr_hash_get(round_info_ht_, local_rid_ptr, sizeof(roundid_t));
    // it should be a new round.
    SAFE_ASSERT(rinfo_ptr == NULL);

    rinfo_ptr = malloc(sizeof(round_info_t));
    round_info_init(rinfo_ptr);
    rinfo_ptr->rid = local_rid_ptr;

    for (size_t i = 0; i < rids_len; i++) {
        // every rid in the vector is involved at default.
        // set up remote_rid -> local_rid mapping
        // TODO no need to save for key
        roundid_t *remote_rid_ptr = apr_pcalloc(rinfo_ptr->round_pool, sizeof(roundid_t));
        remote_rid_ptr->gid = rids_ptr[i]->gid;
        remote_rid_ptr->sid = rids_ptr[i]->sid;
        remote_rid_ptr->bid = rids_ptr[i]->bid;
        // apr_hash_set(roundid_ht_, remote_rid_ptr, sizeof(roundid_t), local_rid_ptr);

        group_info_t *ginfo_ptr = apr_pcalloc(rinfo_ptr->round_pool, sizeof(group_info_t));
        group_info_init(rinfo_ptr, ginfo_ptr);

        // add group to round
        apr_hash_set(rinfo_ptr->group_info_ht, remote_rid_ptr, sizeof(roundid_t), ginfo_ptr);
        // attach round
        apr_hash_set(round_info_ht_, remote_rid_ptr, sizeof(roundid_t), rinfo_ptr);
    }

    pthread_mutex_unlock(&round_info_mutex_);
    return rinfo_ptr;
}

round_info_t *attach_round_info_async(roundid_t **rids, size_t sz_rids, mpaxos_req_t *req) {
    pthread_mutex_lock(&round_info_mutex_);
    // TODO there is a bug here, can you locate it?

    //find local_rid from rid_vec
    groupid_t gid = rids[0]->gid;
    roundid_t *local_rid = NULL;
    for (int i = 0; i < sz_rids; i++) {
        if (rids[i]->gid == gid) {
            local_rid = rids[i];
            break;
        }
    }

    round_info_t *rinfo = apr_hash_get(round_info_ht_, local_rid, sizeof(roundid_t));
    // it should be a new round.
    SAFE_ASSERT(rinfo == NULL);

    rinfo = malloc(sizeof(round_info_t));
    round_info_init(rinfo);
    rinfo->rid = local_rid;
    rinfo->req = req;
    rinfo->rids = rids;
    rinfo->sz_rids = sz_rids;

    for (size_t i = 0; i < sz_rids; i++) {
        // every rid in the vector is involved at default.
        // set up remote_rid -> local_rid mapping
        // TODO no need to save for key
        roundid_t *remote_rid = apr_pcalloc(rinfo->round_pool, sizeof(roundid_t));
        remote_rid->gid = rids[i]->gid;
        remote_rid->sid = rids[i]->sid;
        remote_rid->bid = rids[i]->bid;
        // apr_hash_set(roundid_ht_, remote_rid_ptr, sizeof(roundid_t), local_rid_ptr);

        group_info_t *ginfo_ptr = apr_pcalloc(rinfo->round_pool, sizeof(group_info_t));
        group_info_init(rinfo, ginfo_ptr);

        // add group to round
        apr_hash_set(rinfo->group_info_ht, remote_rid, sizeof(roundid_t), ginfo_ptr);
        // attach round
        apr_hash_set(round_info_ht_, remote_rid, sizeof(roundid_t), rinfo);
    }

    pthread_mutex_unlock(&round_info_mutex_);
    return rinfo;
}

void detach_round_info(round_info_t *round_info_ptr) {
    pthread_mutex_lock(&round_info_mutex_);

    apr_hash_index_t *hi = NULL;
    for (hi = apr_hash_first(round_info_ptr->round_pool, round_info_ptr->group_info_ht);
        hi; hi = apr_hash_next(hi)) {
        roundid_t *p_rid = NULL;
        group_info_t *ginfo_ptr = NULL;
        apr_hash_this(hi, (const void**)&p_rid, NULL, (void**)&ginfo_ptr);

        group_info_final(ginfo_ptr);
        apr_hash_set(round_info_ht_, p_rid, sizeof(roundid_t), NULL);
    }
    apr_hash_clear(round_info_ptr->group_info_ht);
    pthread_mutex_unlock(&round_info_mutex_);
    round_info_final(round_info_ptr);
    free(round_info_ptr);
}

round_info_t* get_round_info(roundid_t *remote_rid) {
//  pthread_mutex_lock(&round_info_mutex_);
    roundid_t rid;
    memset(&rid, 0, sizeof(roundid_t));
    rid.gid = remote_rid->gid;
    rid.sid = remote_rid->sid;
    rid.bid = remote_rid->bid;

    round_info_t *rinfo_ptr;
    rinfo_ptr = apr_hash_get(round_info_ht_, &rid, sizeof(roundid_t));
    //SAFE_ASSERT(local_rid_ptr != NULL);

//  pthread_mutex_unlock(&round_info_mutex_);
  return rinfo_ptr;
}

void broadcast_msg_prepare(groupid_t gid,
    roundid_t **rids_ptr, size_t rids_size) {

    Mpaxos__MsgPrepare msg_prep = MPAXOS__MSG_PREPARE__INIT;
    Mpaxos__MsgHeader header = MPAXOS__MSG_HEADER__INIT;
    Mpaxos__ProcessidT pid = MPAXOS__PROCESSID_T__INIT;

    msg_prep.h = &header;
    msg_prep.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__PREPARE;
    // set pid
    msg_prep.h->pid = &pid;
    msg_prep.h->pid->gid = gid;
    msg_prep.h->pid->nid = get_local_nid();
    // set rids
    msg_prep.n_rids = rids_size;
    msg_prep.rids = rids_ptr;


    // send the message
    groupid_t* gids = (groupid_t*)malloc(rids_size * sizeof(groupid_t));
    for (int i = 0; i < rids_size; i++) {
        gids[i] = rids_ptr[i]->gid;
    }

    // This is calculated packing length
    size_t sz_msg = mpaxos__msg_prepare__get_packed_size (&msg_prep);
    uint8_t *buf = (uint8_t *)malloc(sz_msg);
    mpaxos__msg_prepare__pack(&msg_prep, buf);
    log_message_rid("broadcast", "PREPARE", msg_prep.rids, msg_prep.n_rids, sz_msg);
    send_to_groups(gids, rids_size, (const char *)buf, sz_msg);

    free(buf);
    free(gids);
}

//TODO check, feel something wrong.
void broadcast_msg_accept(groupid_t gid,
    round_info_t *round_info_ptr,
    proposal *prop_p) {
    Mpaxos__MsgAccept msg_accp = MPAXOS__MSG_ACCEPT__INIT;
    Mpaxos__MsgHeader header = MPAXOS__MSG_HEADER__INIT;
    Mpaxos__ProcessidT pid = MPAXOS__PROCESSID_T__INIT;
    msg_accp.h = &header;
    msg_accp.h->pid = &pid;
    msg_accp.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPT;
    msg_accp.h->pid->gid = gid;
    msg_accp.h->pid->nid = get_local_nid();
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
    log_message_rid("broadcast", "ACCEPT", prop_p->rids, prop_p->n_rids, sz_msg);
    char *buf = (char *)malloc(sz_msg);
    mpaxos__msg_accept__pack(&msg_accp, (uint8_t *)buf);
    send_to_groups(gids, prop_p->n_rids, buf, sz_msg);

    free(buf);
    free(gids);
}
