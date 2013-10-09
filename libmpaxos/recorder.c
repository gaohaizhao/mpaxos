
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <json/json.h>
#include <apr_hash.h>
#include <apr_queue.h>
#include <apr_thread_pool.h>
#include "mpaxos/mpaxos.h"
#include "proposer.h"
#include "acceptor.h"
#include "view.h"
#include "slot_mgr.h"
#include "utils/logger.h"
#include "utils/mtime.h"
#include "utils/mlock.h"
#include "comm.h"
#include "async.h"


apr_pool_t *mp_recorder_;
apr_hash_t *ht_value_;        //instid_t -> value_t
apr_hash_t *ht_newest_;
apr_thread_mutex_t *mx_value_;

void recorder_init() {
    apr_pool_create(&mp_recorder_, NULL);
    ht_value_ = apr_hash_make(mp_recorder_);
    ht_newest_ = apr_hash_make(mp_recorder_);    
    apr_thread_mutex_create(&mx_value_, APR_THREAD_MUTEX_UNNESTED, mp_recorder_);
}

void recorder_destroy() {
    apr_pool_destroy(mp_recorder_);
}

int get_instval(uint32_t gid, uint32_t in, char* buf,
        uint32_t bufsize, uint32_t *val_size) {
    //TODO [FIX]
    return 0;
}

/*
bool has_value(groupid_t gid, slotid_t sid) {
    instid_t *p_iid = (instid_t *) calloc(1, sizeof(instid_t));
    p_iid->gid = gid;
    p_iid->sid = sid;

    value_t* v = apr_hash_get(ht_value_, p_iid, sizeof(instid_t));

    free(p_iid);
    return (v != NULL);
}
*/

void record_proposal(proposal_t *prop) {
    apr_thread_mutex_lock(mx_value_);
    proposal_t *p = apr_palloc(mp_recorder_, sizeof(proposal_t));
    prop_cpy(p, prop, mp_recorder_);
    
    for (int i = 0; i < p->n_rids; i++) {
        roundid_t *rid = p->rids[i];
        instid_t *iid = (instid_t *) apr_pcalloc(mp_recorder_, sizeof(instid_t));
        iid->gid = rid->gid;
        iid->sid = rid->sid;
        apr_hash_set(ht_value_, iid, sizeof(instid_t), prop);
        
        // renew the newest value number
        slotid_t *sid_old = apr_hash_get(ht_newest_, &rid->gid, sizeof(groupid_t));    
        if (sid_old == NULL || *sid_old < rid->sid) {
            // do something
            apr_hash_set(ht_newest_, &rid->gid, sizeof(groupid_t), &rid->sid);
        }
    }
    
    apr_thread_mutex_unlock(mx_value_);
}

//int put_instval(groupid_t gid, slotid_t sid, uint8_t *data,
//        size_t sz_data) {
//    apr_thread_mutex_lock(mx_value_);
//    
//    instid_t *iid = (instid_t *) apr_pcalloc(mp_recorder_, sizeof(instid_t));
//    iid->gid = gid;
//    iid->sid = sid;
//    value_t *val = (value_t *) apr_palloc(mp_recorder_, sizeof(value_t));
//    val->data = (uint8_t *) apr_palloc(mp_recorder_, sz_data);
//    val->len = sz_data;
//    memcpy (val->data, data, sz_data);
//
//    // TODO [IMPROVE] just put in memory now, should be put in bdb.
//    apr_hash_set(ht_value_, iid, sizeof(instid_t), val);
//
//    // renew the newest value number
//    slotid_t *sid_old = apr_hash_get(ht_newest_, &gid, sizeof(groupid_t));    
//    if (sid_old == NULL || *sid_old < sid) {
//        // do something
//        groupid_t *g_new = apr_pcalloc(mp_recorder_, sizeof(groupid_t));
//        slotid_t *s_new = apr_pcalloc(mp_recorder_, sizeof(slotid_t));
//        *g_new = gid;
//        *s_new = sid;
//        apr_hash_set(ht_newest_, g_new, sizeof(groupid_t), s_new);
//    }
//
//    // TODO [IMPROVE] free some space in the map.
//    // forget
//    acceptor_forget();
//    apr_thread_mutex_unlock(mx_value_);
//    return 0;
//}

slotid_t get_newest_sid(groupid_t gid, int *is_me) {
    apr_thread_mutex_lock(mx_value_);
    slotid_t *s = apr_hash_get(ht_newest_, &gid, sizeof(groupid_t));    
    slotid_t sid = (s == NULL) ? 0: *s;
    instid_t iid;
    memset(&iid, 0, sizeof(instid_t));
    iid.gid = gid;
    iid.sid = sid;
    if (sid == 0) {
        // never seen value before.
        *is_me = 0;
    } else {
        proposal_t *prop = apr_hash_get(ht_value_, &iid, sizeof(instid_t));
        SAFE_ASSERT(prop != NULL);
        *is_me = (prop->nid == get_local_nid()) ? 1 : 0;
    }
    
    LOG_DEBUG("newest sid %lu for gid %lu", sid, gid);
    apr_thread_mutex_unlock(mx_value_);
    return sid;
}
