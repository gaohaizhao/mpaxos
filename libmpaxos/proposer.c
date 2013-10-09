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
#include "slot_mgr.h"


static apr_pool_t *mp_prop_;
static apr_hash_t *ht_round_info_; //roundid_t -> round_info

static pthread_mutex_t mx_round_info_;

static pthread_mutex_t *group_mutexs_;

void lock_group(groupid_t gid) {
    pthread_mutex_lock(group_mutexs_ + gid);
}

void unlock_group(groupid_t gid) {
    pthread_mutex_unlock(group_mutexs_ + gid);
}

void group_info_init(round_info_t *r, group_info_t *g) {
    g->promise_ht = apr_hash_make(r->mp);
    g->accepted_ht = apr_hash_make(r->mp);
    g->n_promises_yes = 0;
    g->n_promises_no = 0;
    g->n_accepteds_yes = 0;
    g->n_accepteds_no = 0;
    g->max_bid_prop_ptr = NULL;
    pthread_mutex_init(&g->mutex, NULL);
}

void group_info_final(group_info_t *g) {
    if (g->max_bid_prop_ptr != NULL) {
        //TODO destroy
        prop_destroy(g->max_bid_prop_ptr);
/*
        free(g->max_bid_prop_ptr);
*/
    }
}

void round_info_init(round_info_t *r) {
    apr_pool_create(&r->mp, NULL);
    apr_thread_mutex_create(&r->mx, APR_THREAD_MUTEX_UNNESTED, r->mp);
//    apr_thread_cond_create(&r->cond_accp, r->round_pool);
//    apr_thread_cond_create(&r->cond_prep, r->round_pool);
    r->group_info_ht = apr_hash_make(r->mp);
    r->after_phase1 = 0;
    r->after_phase2 = 0;
    r->is_voriginal = 0;
    r->is_good = 0;
    r->prop_max = NULL;
    r->prop_self = NULL;
}

void round_info_final(round_info_t *r) {
    r->rid = NULL;
/*
    SAFE_ASSERT(pthread_mutex_destroy(&r->mutex) == 0);
    SAFE_ASSERT(pthread_cond_destroy(&r->prep_cond) == 0);
    SAFE_ASSERT(pthread_cond_destroy(&r->accp_cond) == 0);
*/
    for (int i = 0; i < r->sz_rids; i++) {
        free(r->rids[i]);
    }
    free(r->rids);
    apr_pool_destroy(r->mp);
    free(r);
}

void proposer_init() {
    // initialize the hash table
    apr_pool_create(&mp_prop_, NULL);
    ht_round_info_ = apr_hash_make(mp_prop_);

    SAFE_ASSERT(pthread_mutex_init(&mx_round_info_, NULL) == 0);


  //TODO
    int n_groups = 10;
    group_mutexs_ = malloc((n_groups + 1) * sizeof(pthread_mutex_t));
    for (int i = 1; i <= n_groups; i++) {
        pthread_mutex_init(group_mutexs_ + i, NULL);
    }

    LOG_INFO("proposer created");
}

void proposer_destroy() {
    SAFE_ASSERT(pthread_mutex_destroy(&mx_round_info_) == 0);
    LOG_INFO("proposer destroyed");
  
    //TODO
    int n_groups = 10;
    for (int i = 1; i <= n_groups; i++) {
      pthread_mutex_destroy(group_mutexs_ + i);
    }
    free(group_mutexs_);
    apr_pool_destroy(mp_prop_);
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
int check_majority(round_info_t *rinfo,
        bool check_promise, bool check_accepted) {
    // check majority for each group, and choose what we will propose

    apr_hash_index_t *hi;
    SAFE_ASSERT(rinfo->group_info_ht != NULL);
    int ret = MAJORITY_YES;
    for (hi = apr_hash_first(rinfo->mp, rinfo->group_info_ht); hi; hi = apr_hash_next(hi)) {
        roundid_t *rid_ptr;
        group_info_t *ginfo_ptr;
        apr_hash_this(hi, (const void**)&rid_ptr, NULL, (void **)&ginfo_ptr);
        int gsz = get_group_size(rid_ptr->gid);
        if (check_promise) {
            if (ginfo_ptr->n_promises_no >= ((gsz >> 1) + 1)) {
                ret = MAJORITY_NO;
                break;
            } else if (ginfo_ptr->n_promises_yes < ((gsz >> 1) + 1)) {
//              LOG_DEBUG(": no majority for group ", gid);
                ret = MAJORITY_UNCERTAIN;
                break;
            }
        }

        if (check_accepted) {
            if (ginfo_ptr->n_accepteds_no >= ((gsz >> 1) + 1)) {
                ret = MAJORITY_NO;
                break;
            } else if (ginfo_ptr->n_accepteds_yes < ((gsz >> 1) + 1)) {
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


int check_majority_ex(round_info_t *round_info,
        bool check_promise, bool check_accepted) {
    apr_thread_mutex_lock(round_info->mx);
    int ret = check_majority(round_info, check_promise, check_accepted);
    apr_thread_mutex_unlock(round_info->mx);
    return ret;
}


void handle_msg_promise(msg_promise_t *msg_prom) {
    SAFE_ASSERT(msg_prom->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__PROMISE);
    SAFE_ASSERT(msg_prom->n_ress > 0);

    pthread_mutex_lock(&mx_round_info_);


    round_info_t *rinfo = NULL;
    for (int i = 0; i < msg_prom->n_ress; i++) {
        response_t *res_ptr = msg_prom->ress[i];
        roundid_t remote_rid;
        memset(&remote_rid, 0, sizeof(remote_rid));
        
        remote_rid.gid = res_ptr->rid->gid;
        remote_rid.sid = res_ptr->rid->sid;
        remote_rid.bid = res_ptr->rid->bid;
        round_info_t *info = get_round_info(&remote_rid);
        
        if (info == NULL) {
            LOG_DEBUG("no such round, message too old or too future");
            continue;
        } else {
            if (rinfo == NULL) {
                rinfo = info;
            } else {
                // currently we only handle those who have save round infos.
                SAFE_ASSERT(rinfo == info);
            }
        }

        // handle this round info.
        apr_thread_mutex_lock(rinfo->mx);

        // find this group
        group_info_t *group_info;
        group_info = apr_hash_get(rinfo->group_info_ht,
                &remote_rid, sizeof(roundid_t));

        // do a not-null check
        SAFE_ASSERT(group_info != NULL);

        // adjust number of promises
        nodeid_t *nid_ptr = apr_pcalloc(rinfo->mp, sizeof(nodeid_t));
        nid_ptr = &msg_prom->h->pid->nid;
        ack_enum *re = apr_hash_get(group_info->promise_ht, nid_ptr, sizeof(nodeid_t));
        if (re == NULL) {
            re = apr_pcalloc(rinfo->mp, sizeof(ack_enum));
            // juset set it to some number, just different from success or deny 
            *re = 100;
        }

        if (res_ptr->ack == MPAXOS__ACK_ENUM__SUCCESS) {
            LOG_DEBUG("received a yes for prepare.");
            if (*re != MPAXOS__ACK_ENUM__SUCCESS) {
                group_info->n_promises_yes++;
            }
            *re = MPAXOS__ACK_ENUM__SUCCESS;
        } else if (res_ptr->ack == MPAXOS__ACK_ENUM__ERR_BID) {
            LOG_DEBUG("received a no for prepare");
            // adjust max ballot id proposal
            if (*re != MPAXOS__ACK_ENUM__ERR_BID) {
                group_info->n_promises_no++;
            }
            *re = MPAXOS__ACK_ENUM__ERR_BID;
        }
        apr_hash_set(group_info->promise_ht, nid_ptr, sizeof(nodeid_t), re);

        if (res_ptr->n_props > 0) {
            //Already accepted some proposals
            for (int j = 0; j < res_ptr->n_props; j++) {
                if (group_info->max_bid_prop_ptr == NULL ||
                        group_info->max_bid_prop_ptr->rids[0]->bid
                                                < res_ptr->props[j]->rids[0]->bid) {


                    if (group_info->max_bid_prop_ptr != NULL) {
                        prop_destroy(group_info->max_bid_prop_ptr);
/*
                        free(group_info_ptr->max_bid_prop_ptr);
*/
                    }
                    group_info->max_bid_prop_ptr = malloc(sizeof(proposal_t));
                    prop_cpy(group_info->max_bid_prop_ptr, res_ptr->props[j], rinfo->mp);
                    // [IMPORTANT] assume that they have the same rids.
                    if (rinfo->prop_max == NULL || group_info->max_bid_prop_ptr->rids[0]->bid > rinfo->prop_max->rids[0]->bid) {
                        rinfo->prop_max = group_info->max_bid_prop_ptr;
                    }
                }
            }
        }
        // check if we can signal this round to stop waiting
        apr_thread_mutex_unlock(rinfo->mx);
    }
    pthread_mutex_unlock(&mx_round_info_);
    if (rinfo != NULL) {
        int ret_majority = MAJORITY_UNCERTAIN;
        ret_majority = check_majority_ex(rinfo, true, false);
/*
        //for_debug
        SAFE_ASSERT(ret_majority == MAJORITY_YES);
*/
        if (ret_majority == MAJORITY_YES) {
            LOG_DEBUG("recevie majority yes for prepare, ready go into phase2.");
            // apr_status_t status = apr_thread_cond_signal(round_info_ptr->cond_prep);
            // SAFE_ASSERT(status == APR_SUCCESS);
            phase_1_async_after(rinfo);
        } else if (ret_majority == MAJORITY_NO) {
            // something is wrong. either:
            // 1. there are some live locks, and we should retry.
            // 2. this slot is already decided.
            // TODO
            LOG_DEBUG("meet a majority no in phase 1.");
            // i am gonna do something, and can only do once.
            bool is_good = false;
            if (rinfo->is_good == 0) {
                is_good = true;
                rinfo->is_good = 1;
            }
            if (is_good) {
                rinfo->req->n_retry ++;
                start_round_async(rinfo->req);
            }
        } else if (ret_majority == MAJORITY_UNCERTAIN) {
            LOG_DEBUG("not ready to go into phase 2.");
        } else {
            SAFE_ASSERT(0);
        }
    } else {
/*
        // for_debug
        SAFE_ASSERT(0);
*/
    }
}

void handle_msg_accepted(msg_accepted_t *msg) {
	SAFE_ASSERT(msg->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPTED);
	SAFE_ASSERT(msg->n_ress > 0);

    pthread_mutex_lock(&mx_round_info_);

    round_info_t *rinfo = NULL;
    for (int i = 0; i < msg->n_ress; i++) {
    	response_t *res_ptr = msg->ress[i];
    	roundid_t remote_rid;
        memset(&remote_rid, 0, sizeof(roundid_t));
        
        remote_rid.gid = res_ptr->rid->gid;
        remote_rid.sid = res_ptr->rid->sid;
        remote_rid.bid = res_ptr->rid->bid;
        rinfo = get_round_info(&remote_rid);

        if (rinfo == NULL) {
        	LOG_DEBUG("no such round, message too old or too future. "
                    "gid: %lu, sid: %lu, bid: %lu", remote_rid.gid, remote_rid.sid, remote_rid.bid);
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
        nodeid_t *nid_ptr = apr_pcalloc(rinfo->mp, sizeof(nodeid_t));
        nid_ptr = &msg->h->pid->nid;
        ack_enum *re = apr_hash_get(group_info_ptr->accepted_ht,
                nid_ptr, sizeof(nodeid_t));
        if (re == NULL ) {
            re = apr_pcalloc(rinfo->mp, sizeof(ack_enum));
            *re = 100;
        }

        if (res_ptr->ack == MPAXOS__ACK_ENUM__SUCCESS) {
            if (*re != MPAXOS__ACK_ENUM__SUCCESS) {
                group_info_ptr->n_accepteds_yes ++;
            }
            *re = MPAXOS__ACK_ENUM__SUCCESS;
        } else if (res_ptr->ack == MPAXOS__ACK_ENUM__ERR_BID) {
            // adjust max ballot id proposal
            if (*re != MPAXOS__ACK_ENUM__ERR_BID) {
                group_info_ptr->n_accepteds_no ++;
            }
            *re = MPAXOS__ACK_ENUM__ERR_BID;
        }
        apr_hash_set(group_info_ptr->accepted_ht, nid_ptr, sizeof(nodeid_t), re);
        apr_thread_mutex_unlock(rinfo->mx);

        // If I receive an msg_accepted, I must have sent a proposal.
        // I should have remembered the proposal somewhere.
        // TODO bother to check.

        // check if we can signal this round to stop waiting
    }
    
    // TODO [fix] there is something wrong with the lock.
    pthread_mutex_unlock(&mx_round_info_);
    if (rinfo != NULL) {
        int ret_majority = MAJORITY_UNCERTAIN;
        ret_majority = check_majority_ex(rinfo, false, true);
        if (ret_majority == MAJORITY_YES) {
            LOG_DEBUG("recevie majority yes for accept, ready go into phase3.");
            // apr_status_t status = apr_thread_cond_signal(round_info_ptr->cond_prep);
            // SAFE_ASSERT(status == APR_SUCCESS);
            phase_2_async_after(rinfo);
        } else if (ret_majority == MAJORITY_NO) {
            // something is wrong. either:
            // 1. there are some live locks, and we should retry.
            // 2. this slot is already decided.
            // TODO
            LOG_DEBUG("meet a majority no in phase 2.");
            // i am gonna do something, and can only do once.
            apr_thread_mutex_lock(rinfo->mx);
            bool is_good = false;
            if (rinfo->is_good == 0) {
                is_good = true;
                rinfo->is_good = 1;
            }
            apr_thread_mutex_unlock(rinfo->mx);
            if (is_good) {
                rinfo->req->n_retry ++;
                start_round_async(rinfo->req);
            } else {
                // do nothing
            }
        } else if (ret_majority == MAJORITY_UNCERTAIN) {
            LOG_DEBUG("not ready to go into phase 3.");
        } else {
            SAFE_ASSERT(0);
        }
    } else {
/*
        // for_debug
        SAFE_ASSERT(0);
*/
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
    
    if (rinfo->prop_max != NULL) {
        proposal_t prop = MPAXOS__PROPOSAL__INIT;
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
        broadcast_msg_accept(rinfo->rid->gid, rinfo, &prop);
    } else {
        proposal_t *prop_self = apr_palloc(rinfo->mp, sizeof(proposal_t));
        mpaxos__proposal__init(prop_self);
        prop_self->n_rids = rinfo->sz_rids;
        prop_self->rids = rinfo->rids;
        prop_self->value.data = rinfo->req->data;
        prop_self->value.len = rinfo->req->sz_data;
        prop_self->nid = get_local_nid();
        broadcast_msg_accept(rinfo->rid->gid, rinfo, prop_self);
    }
    
	/*----------------------------- phase II end ----------------------------*/
	// wait
	// sleep_and_wait(round_info->cond_accp, round_info->mx, timeout);
	apr_thread_mutex_unlock(rinfo->mx);
	return 0;
}

/**
 * 
 * @param req
 * @return 
 */
int start_round_async(mpaxos_req_t *req) {
    if (req->n_retry >0) {
        LOG_DEBUG("retry a request");
    }
    roundid_t **rids = NULL;
    rids = (roundid_t **) malloc(req->sz_gids * sizeof(roundid_t *));
    
    int is_saveprep = 1;
    for (int i = 0; i < req->sz_gids; i++) {
        rids[i] = (roundid_t *)malloc(sizeof(roundid_t));
        mpaxos__roundid_t__init(rids[i]);
        rids[i]->gid = (req->gids[i]);

        //get_insnum(gids[i], &sid_ptr);
        //*sid_ptr += 1;
        rids[i]->bid = (1 + req->n_retry);
        //rids[i]->sid = acquire_slot(req->gids[i], get_local_nid());
        int is_me = 0;
        rids[i]->sid = get_newest_sid(req->gids[i], &is_me);
        if (!is_me) {
            is_saveprep = 0;
        }
    }

    round_info_t *rinfo = attach_round_info_async(rids, req->sz_gids, req);
    LOG_DEBUG("Start phase 1 prepare.");
    phase_1_async(rinfo);
    return 0;
}


int phase_1_async_after(round_info_t *rinfo) {
    // check majority for each group, and choose what we will propose
    apr_thread_mutex_lock(rinfo->mx);
    bool go = false;
    if (rinfo->after_phase1 == 0) {
        rinfo->after_phase1 = 1;
        go = true;
    }
	int ret = check_majority(rinfo, true, false);
    SAFE_ASSERT(ret == MAJORITY_YES);
    apr_thread_mutex_unlock(rinfo->mx);

    LOG_DEBUG("Start phase 2 accept.");
    // TODO [fix]
    if (go) {
        phase_2_async(rinfo);
    }
    return 0;
}

int phase_2_async_after(round_info_t *rinfo) {
    /*-------------------------- learn and decide ---------------------------*/
    if (rinfo->after_phase2 == 1) {
        return 0;
    }
	rinfo->after_phase2 = 1;

    // check majority for each group for accept
    int ret = check_majority_ex(rinfo, false, true);
    SAFE_ASSERT(ret == MAJORITY_YES);

    LOG_DEBUG("all done. decide a value.");

    
    rinfo->req->sids = malloc(rinfo->sz_rids * sizeof(slotid_t));
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
    proposal_t *prop = (rinfo->prop_max != NULL) ? rinfo->prop_max : rinfo->prop_self;
    record_proposal(prop);
    
    // send LEARNED to everybody.
    // broadcast_msg_decide(rinfo);
    
    int is_vo = rinfo->is_voriginal;
    mpaxos_req_t *req = rinfo->req;
    
    detach_round_info(rinfo);

    if (is_vo!= 0) {
        LOG_DEBUG("this is not the original value i propose, so propose again.");
        req->n_retry ++;
        start_round_async(req);
    } else {
        // go to call back
        async_ready_callback(req);
    }
    return 0;
}


round_info_t *attach_round_info_async(roundid_t **rids, size_t sz_rids, mpaxos_req_t *req) {
    pthread_mutex_lock(&mx_round_info_);
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

    round_info_t *rinfo = apr_hash_get(ht_round_info_, local_rid, sizeof(roundid_t));
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
        roundid_t *remote_rid = apr_pcalloc(rinfo->mp, sizeof(roundid_t));
        remote_rid->gid = rids[i]->gid;
        remote_rid->sid = rids[i]->sid;
        remote_rid->bid = rids[i]->bid;
        // apr_hash_set(roundid_ht_, remote_rid_ptr, sizeof(roundid_t), local_rid_ptr);

        group_info_t *ginfo_ptr = apr_pcalloc(rinfo->mp, sizeof(group_info_t));
        group_info_init(rinfo, ginfo_ptr);

        // add group to round
        apr_hash_set(rinfo->group_info_ht, remote_rid, sizeof(roundid_t), ginfo_ptr);
        // attach round
        apr_hash_set(ht_round_info_, remote_rid, sizeof(roundid_t), rinfo);
    }

    pthread_mutex_unlock(&mx_round_info_);
    return rinfo;
}

void detach_round_info(round_info_t *round_info) {
    pthread_mutex_lock(&mx_round_info_);

    apr_hash_index_t *hi = NULL;
    for (hi = apr_hash_first(round_info->mp, round_info->group_info_ht);
        hi; hi = apr_hash_next(hi)) {
        roundid_t *p_rid = NULL;
        group_info_t *ginfo_ptr = NULL;
        apr_hash_this(hi, (const void**)&p_rid, NULL, (void**)&ginfo_ptr);

        group_info_final(ginfo_ptr);
        apr_hash_set(ht_round_info_, p_rid, sizeof(roundid_t), NULL);
    }
    apr_hash_clear(round_info->group_info_ht);
    pthread_mutex_unlock(&mx_round_info_);
    round_info_final(round_info);
}

round_info_t* get_round_info(roundid_t *remote_rid) {
//  pthread_mutex_lock(&round_info_mutex_);
    roundid_t rid;
    memset(&rid, 0, sizeof(roundid_t));
    rid.gid = remote_rid->gid;
    rid.sid = remote_rid->sid;
    rid.bid = remote_rid->bid;

    round_info_t *rinfo = NULL;
    rinfo = apr_hash_get(ht_round_info_, &rid, sizeof(roundid_t));
    
/*
    // for_debug
    if (rinfo == NULL) {
        LOG_ERROR("gid: %ld, sid: %ld, bid:%ld", rid.gid, rid.sid, rid.bid);
        SAFE_ASSERT(rinfo != NULL);
    }
*/

//  pthread_mutex_unlock(&round_info_mutex_);
    return rinfo;
}

void broadcast_msg_prepare(groupid_t gid,
    roundid_t **rids_ptr, size_t rids_size) {

    msg_prepare_t msg_prep = MPAXOS__MSG_PREPARE__INIT;
    msg_header_t header = MPAXOS__MSG_HEADER__INIT;
    processid_t pid = MPAXOS__PROCESSID_T__INIT;

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
    log_message_rid("broadcast", "PREPARE", msg_prep.h, msg_prep.rids, msg_prep.n_rids, sz_msg);
    send_to_groups(gids, rids_size, MSG_PREPARE, (const char *)buf, sz_msg);

    free(buf);
    free(gids);
}

//TODO check, feel something wrong.
void broadcast_msg_accept(groupid_t gid,
    round_info_t *rinfo,
    proposal_t *prop_p) {
    msg_accept_t msg_accp = MPAXOS__MSG_ACCEPT__INIT;
    msg_header_t header = MPAXOS__MSG_HEADER__INIT;
    processid_t pid = MPAXOS__PROCESSID_T__INIT;
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
    log_message_rid("broadcast", "ACCEPT", msg_accp.h, prop_p->rids, prop_p->n_rids, sz_msg);
    char *buf = (char *)malloc(sz_msg);
    mpaxos__msg_accept__pack(&msg_accp, (uint8_t *)buf);
    send_to_groups(gids, prop_p->n_rids, MSG_ACCEPT, buf, sz_msg);

    free(buf);
    free(gids);
}


//TODO check, feel something wrong.
void broadcast_msg_decide(round_info_t *rinfo) {
    proposal_t prop = MPAXOS__PROPOSAL__INIT;
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
    
    msg_decide_t msg_dcd = MPAXOS__MSG_DECIDE__INIT;
    msg_header_t header = MPAXOS__MSG_HEADER__INIT;
    processid_t pid = MPAXOS__PROCESSID_T__INIT;
    msg_dcd.h = &header;
    msg_dcd.h->pid = &pid;
    msg_dcd.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__DECIDE;
    msg_dcd.h->pid->gid = rinfo->rids[0]->gid;
    msg_dcd.h->pid->nid = get_local_nid();
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
