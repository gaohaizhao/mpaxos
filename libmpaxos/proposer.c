/*
 * proposer.cc
 *
 *  Created on: Jan 2, 2013
 *      Author: ms
 */

#include <stdio.h>
#include <errno.h>
#include <memory.h>
#include <pthread.h>

#include "comm.h"
#include "proposer.h"
#include "mpaxos/mpaxos.h"
#include "utils/logger.h"
#include "utils/mtime.h"
#include "log_helper.h"
#include "view.h"


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
    pthread_cond_init(&r->prep_cond, NULL);
    pthread_cond_init(&r->accp_cond, NULL);
    pthread_mutex_init(&r->mutex, NULL);
    apr_pool_create(&r->round_pool, NULL);
    r->group_info_ht = apr_hash_make(r->round_pool);
}

void round_info_final(round_info_t *r) {
    r->rid_ptr = NULL;
    SAFE_ASSERT(pthread_mutex_destroy(&r->mutex) == 0);
    SAFE_ASSERT(pthread_cond_destroy(&r->prep_cond) == 0);
    SAFE_ASSERT(pthread_cond_destroy(&r->accp_cond) == 0);
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

bool check_majority(round_info_t *rinfo_t,
        bool check_promise, bool check_accepted) {
    // check majority for each group, and choose what we will propose

    apr_hash_index_t *hi;
    SAFE_ASSERT(rinfo_t->group_info_ht != NULL);
    for (hi = apr_hash_first(rinfo_t->round_pool, rinfo_t->group_info_ht); hi; hi = apr_hash_next(hi)) {
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


bool check_majority_ex(round_info_t *rinfo_t,
        bool check_promise, bool check_accepted) {
    pthread_mutex_lock(&rinfo_t->mutex);
    bool ret = check_majority(rinfo_t, check_promise, check_accepted);
    pthread_mutex_unlock(&rinfo_t->mutex);
    return ret;
}


void handle_msg_promise(msg_promise_t *msg_prom_ptr) {
    SAFE_ASSERT(msg_prom_ptr->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__PROMISE);
    SAFE_ASSERT(msg_prom_ptr->n_ress > 0);

    pthread_mutex_lock(&round_info_mutex_);


  for (int i = 0; i < msg_prom_ptr->n_ress; i++) {
    response_t *res_ptr = msg_prom_ptr->ress[i];
      roundid_t remote_rid;
        memset(&remote_rid, 0, sizeof(remote_rid));
        
        remote_rid.gid = res_ptr->rid->gid;
        remote_rid.sid = res_ptr->rid->sid;
        remote_rid.bid = res_ptr->rid->bid;
      round_info_t *round_info_ptr = get_round_info(&remote_rid);

      if (round_info_ptr == NULL) {
        LOG_DEBUG(": no such round, message too old or too future");
        continue;
      }

      // handle this round info.
        pthread_mutex_lock(&round_info_ptr->mutex);

        // find this group
        group_info_t *group_info_ptr;
        group_info_ptr = apr_hash_get(round_info_ptr->group_info_ht,
                &remote_rid, sizeof(roundid_t));

        // do a not-null check
        SAFE_ASSERT(group_info_ptr != NULL);

        // adjust number of promises
        nodeid_t *nid_ptr = apr_pcalloc(round_info_ptr->round_pool, sizeof(nodeid_t));
        nid_ptr = &msg_prom_ptr->h->pid->nid;
        ack_enum *re = apr_hash_get(group_info_ptr->promise_ht, nid_ptr, sizeof(nodeid_t));
        if (re == NULL) {
            re = apr_pcalloc(round_info_ptr->round_pool, sizeof(ack_enum));
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
                    prop_cpy(group_info_ptr->max_bid_prop_ptr, res_ptr->props[j], round_info_ptr->round_pool);
                }
            }
        }

        // check if we can signal this round to stop waiting
        bool sig = check_majority(round_info_ptr, true, false);

        if (sig) {
            LOG_DEBUG("wake up the waiting thread.");
            SAFE_ASSERT(pthread_cond_signal(&round_info_ptr->prep_cond) == 0);
        } else {
            LOG_DEBUG("not to wake up the waiting thread.");
        }

        pthread_mutex_unlock(&round_info_ptr->mutex);
  }
  pthread_mutex_unlock(&round_info_mutex_);
}

void handle_msg_accepted(Mpaxos__MsgAccepted *msg) {
  SAFE_ASSERT(msg->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPTED);
  SAFE_ASSERT(msg->n_ress > 0);

    pthread_mutex_lock(&round_info_mutex_);

  for (int i = 0; i < msg->n_ress; i++) {
    response_t *res_ptr = msg->ress[i];
      roundid_t remote_rid;
        memset(&remote_rid, 0, sizeof(roundid_t));
        remote_rid.gid = res_ptr->rid->gid;
        remote_rid.sid = res_ptr->rid->sid;
        remote_rid.bid = res_ptr->rid->bid;
      round_info_t *round_info_ptr = get_round_info(&remote_rid);

      if (round_info_ptr == NULL) {
        LOG_DEBUG("no such round, message too old or too future");
            continue;
      }

      // handle this round info.
        pthread_mutex_lock(&round_info_ptr->mutex);

        // find this group
        group_info_t *group_info_ptr;
        group_info_ptr = apr_hash_get(round_info_ptr->group_info_ht,
                &remote_rid, sizeof(roundid_t));
        // do a not-null check
        SAFE_ASSERT(group_info_ptr != NULL);


        // adjust number of promises
        nodeid_t *nid_ptr = apr_pcalloc(round_info_ptr->round_pool, sizeof(nodeid_t));
        nid_ptr = &msg->h->pid->nid;
        ack_enum *re = apr_hash_get(group_info_ptr->accepted_ht,
                nid_ptr, sizeof(nodeid_t));
        if (re == NULL ) {
            re = apr_pcalloc(round_info_ptr->round_pool, sizeof(ack_enum));
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

        // If I receive an msg_accepted, I must have sent a proposal.
        // I should have remembered the proposal somewhere.
        // TODO bother to check.

        // check if we can signal this round to stop waiting
        bool sig = check_majority(round_info_ptr, false, true);
        if (sig) {
            LOG_DEBUG("wake up thread to decide.");
            SAFE_ASSERT(pthread_cond_signal(&round_info_ptr->accp_cond) == 0);
        }
        pthread_mutex_unlock(&round_info_ptr->mutex);
  }
  pthread_mutex_unlock(&round_info_mutex_);
}

int sleep_and_wait(pthread_cond_t *cond, pthread_mutex_t *mutex, uint32_t timeout) {
    // wait
    struct timespec now, deadline;
    struct timespec ts_begin, ts_end;
    get_realtime(&now);
    //get_realtime(&ts_begin);
    deadline.tv_sec = now.tv_sec + (timeout / 1000);
    deadline.tv_nsec = now.tv_nsec + (timeout % 1000) * 1000000;
    while (deadline.tv_nsec >= 1000000000) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000;
    }
    LOG_DEBUG("go to sleep");
    int ret = pthread_cond_timedwait(cond, mutex, &deadline);
    if (ret != 0 && ret != ETIMEDOUT) {
        LOG_ERROR(": pthread_cond_timedwait() error: ", ret);
    }
    //get_realtime(&ts_end);
    //char out[100];
    //sprintf(out, "sleep %d ms", (ts_end.tv_sec - ts_begin.tv_sec) * 1000 + (ts_end.tv_nsec - ts_begin.tv_nsec) / 1000 / 1000);
    //LOG_DEBUG(out);
    return 0;
}

int phase_1(round_info_t *round_info_ptr, groupid_t gid, ballotid_t bid,
        roundid_t **rids_ptr, size_t rids_len,
        uint8_t *val, size_t val_len, uint32_t timeout) {
    /*------------------------------- phase I ---------------------------------*/
    //The lock must be before broadcasting, otherwise the cond signal for pthread may arrive before it actually goes to sleep.
    pthread_mutex_lock(&round_info_ptr->mutex);

    SAFE_ASSERT(rids_len > 0);
//  SAFE_ASSERT(round_info_ptr->group_info_map.size() == rids_len);

    broadcast_msg_prepare(gid, rids_ptr, rids_len);
    /*------------------------------- phase I end -----------------------------*/
    //wait
    sleep_and_wait(&round_info_ptr->prep_cond, &round_info_ptr->mutex, timeout);

    pthread_mutex_unlock(&round_info_ptr->mutex);
    return 0;
}


int phase_2(round_info_t *round_info_ptr, groupid_t gid, ballotid_t bid,
        roundid_t **rids_ptr, size_t rids_len,
        uint8_t *val, size_t val_len, uint32_t timeout) {
    /*------------------------------- phase II start --------------------------*/
    pthread_mutex_lock(&round_info_ptr->mutex);

    Mpaxos__Proposal prop = MPAXOS__PROPOSAL__INIT;
    prop.n_rids = rids_len;
    prop.rids = rids_ptr;
    prop.value.data = val;
    prop.value.len = val_len;

    broadcast_msg_accept(gid, round_info_ptr, &prop);
    /*----------------------------- phase II end ----------------------------*/

    // wait
    sleep_and_wait(&round_info_ptr->accp_cond, &round_info_ptr->mutex, timeout);

    pthread_mutex_unlock(&round_info_ptr->mutex);
    return 0;
}


/**
 * return
 *  0   success
 *  -1  fail in phase 1 because no majority of promise
 *  -2  fail in phase 2 because no majority of accepted
 */
int run_full_round(groupid_t gid,
        ballotid_t bid,
        roundid_t **rids_ptr,
        size_t rids_len,
        uint8_t* val,
        size_t val_len,
        uint32_t timeout) {
    SAFE_ASSERT(bid != 0);
    LOG_DEBUG("Start to run full round.");


    round_info_t *round_info_ptr = attach_round_info(
              gid, rids_ptr, rids_len);

    LOG_DEBUG("Start phase 1 prepare.");
    phase_1(round_info_ptr, gid, bid, rids_ptr, rids_len, val, val_len, timeout);


    // check majority for each group, and choose what we will propose
    if (!check_majority_ex(round_info_ptr, true, false)) {
        return -1;
    }

    LOG_DEBUG("Start phase 2 accept.");
    phase_2(round_info_ptr, gid, bid, rids_ptr, rids_len, val, val_len, timeout);


    /*-------------------------- learn and decide ---------------------------*/
    // check majority for each group for accept
    if (!check_majority_ex(round_info_ptr, false, true)) {
            return -2;
    }

    LOG_DEBUG("all done. decide.");

    detach_round_info(round_info_ptr);
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
    rinfo_ptr->rid_ptr = local_rid_ptr;

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

round_info_t* get_round_info(roundid_t *remote_rid_ptr) {
//  pthread_mutex_lock(&round_info_mutex_);
    roundid_t rid;
    memset(&rid, 0, sizeof(roundid_t));
    rid.gid = remote_rid_ptr->gid;
    rid.sid = remote_rid_ptr->sid;
    rid.bid = remote_rid_ptr->bid;

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
    log_message_rid("broadcast", "PREPARE", msg_prep.rids, msg_prep.n_rids);


    // send the message
    groupid_t* gids = (groupid_t*)malloc(rids_size * sizeof(groupid_t));
    for (int i = 0; i < rids_size; i++) {
        gids[i] = rids_ptr[i]->gid;
    }

    // This is calculated packing length
    size_t len = mpaxos__msg_prepare__get_packed_size (&msg_prep);
    uint8_t *buf = (uint8_t *)malloc(len);
    mpaxos__msg_prepare__pack(&msg_prep, buf);
    send_to_groups(gids, rids_size, (const char *)buf, len);

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
    log_message_rid("broadcast", "ACCEPT", prop_p->rids, prop_p->n_rids);

    size_t len = mpaxos__msg_accept__get_packed_size (&msg_accp);
    char *buf = (char *)malloc(len);
    mpaxos__msg_accept__pack(&msg_accp, (uint8_t *)buf);
    send_to_groups(gids, prop_p->n_rids, buf, len);

    free(buf);
    free(gids);
}
