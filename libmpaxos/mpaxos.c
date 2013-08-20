/*
 * mpaxos.c
 *
 *  Created on: Dec 30, 2012
 *      Author: ms
 */

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

#define QUEUE_SIZE 1000

// TODO [FIX] a lot of problems


apr_pool_t *pl_global_;
apr_hash_t *val_ht_;        //instid_t -> value_t
apr_hash_t *lastslot_ht_;   //groupid_t -> slotid_t #hash table to store the last slot id that has been called back.

// perhaps we need a large hashtable, probably not
// groupid_t -> some structure
// some structure:
//      a hash table to store value sid->value   ###need to be permanent         
//      last slotid_t that has been called back. ###need to be permanent
//      callback function.      ###cannot be be permanent 
//      a hash table to store callback parameter sid->cb_para    ###permanent?  now not.
//      a mutex for callback. 

pthread_mutex_t value_mutex;

int port_;

void mpaxos_init() {
    apr_initialize();
    apr_pool_create(&pl_global_, NULL);
    lastslot_ht_ = apr_hash_make(pl_global_);
    val_ht_ = apr_hash_make(pl_global_);

    // initialize view
    view_init();

    // initialize comm
    comm_init();

    // initialize proposer
    proposer_init();

    // initialize acceptor
    acceptor_init();
    
    // initialize slot manager
    slot_mgr_init();
    
    // initialize asynchrounous commit and callback module
    mpaxos_async_init(); 

    //start_server(port);

    pthread_mutex_init(&value_mutex, NULL);
}

void mpaxos_start() {
    // start listening
    start_server(port_);
}

void mpaxos_stop() {
    // TODO [improve] when to stop?
}

void mpaxos_destroy() {
    acceptor_final();
    proposer_final();
    comm_final();

    // TODO [IMPROVE] stop eventloop
    stop_server();

    // stop asynchrouns callback.
    mpaxos_async_destroy();
    LOG_DEBUG("LALALA");

    apr_pool_destroy(pl_global_);
    LOG_DEBUG("DU");
    apr_terminate();
    LOG_DEBUG("DUDU");
    pthread_mutex_destroy(&value_mutex);
    LOG_DEBUG("DUDUDU");

}

void set_listen_port(int port) {
    port_ = port;
}

int commit_sync(groupid_t* gids, size_t gid_len, uint8_t *val,
        size_t val_len) {

    // find the appropriate proposer
    // keep propose until success
    LOG_DEBUG("Try to commit.");

    groupid_t gid = gids[0];
    roundid_t **rids_ptr;
    rids_ptr = (roundid_t **) malloc(gid_len * sizeof(roundid_t *));
    for (int i = 0; i < gid_len; i++) {
        rids_ptr[i] = (roundid_t *)malloc(sizeof(roundid_t));
        mpaxos__roundid_t__init(rids_ptr[i]);
        rids_ptr[i]->gid = (gids[i]);

        //get_insnum(gids[i], &sid_ptr);
        //*sid_ptr += 1;
        rids_ptr[i]->bid = (1);
        rids_ptr[i]->sid = acquire_slot(gids[i], get_local_nid());
    }
    do {
        apr_time_t t1 = apr_time_now();

        int rids_len = gid_len;
        int ret = run_full_round(gid, 1, rids_ptr, rids_len, val, val_len, 10000);
        if (ret == 0) {
            apr_time_t t2 = apr_time_now();
            apr_time_t period = t2 - t1;
            if (period > 1 * 1000 * 1000) {
                LOG_WARN("Value committed. Cost time too long: %d", period);
            } else {
                LOG_DEBUG("Value committed. Cost time: %d", period);
            }
            
            // remember, but not invoke call back.
            for (int i = 0; i < rids_len; i++) {
                roundid_t *rid_ptr = rids_ptr[i];
                put_instval(rid_ptr->gid, rid_ptr->sid, val, val_len);
            }

            break;
        } else if (ret == -1) {
            LOG_DEBUG("Error in phase 1?\n");
        } else if (ret == -2) {
            LOG_DEBUG("Error in phase 2?\n");
        }
    } while (0);

    for (int i = 0; i < gid_len; i++) {
        free(rids_ptr[i]);
    }
    free(rids_ptr);
    return 0;
}

void lock_group_commit(groupid_t* gids, size_t sz_gids) {
    char buf[100];
    for (int i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        sprintf(buf, "COMMIT%x", gid);
        m_lock(buf);
    }
}

void unlock_group_commit(groupid_t* gids, size_t sz_gids) {
    char buf[100];
    for (int i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        sprintf(buf, "COMMIT%x", gid);
        m_unlock(buf);
    }
}

/**
 * commit a request that is to be processed asynchronously. add the request to the aync job queue. 
 */
int commit_async(groupid_t* gids, size_t sz_gids, uint8_t *data,
        size_t sz_data, void* cb_para) {
    // call the asynchrounous module.
    mpaxos_async_enlist(gids, sz_gids, data, sz_data, cb_para);
    return 0;
}

pthread_mutex_t add_last_cb_sid_mutex = PTHREAD_MUTEX_INITIALIZER;
int add_last_cb_sid(groupid_t gid) {
    pthread_mutex_lock(&add_last_cb_sid_mutex);
    slotid_t* sid_ptr = apr_hash_get(lastslot_ht_, &gid, sizeof(gid));
    if (sid_ptr == NULL) {
        sid_ptr = apr_palloc(pl_global_, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(pl_global_, sizeof(groupid_t));
        *gid_ptr = gid;
        apr_hash_set(lastslot_ht_, gid_ptr, sizeof(gid), sid_ptr);
    }
    *sid_ptr += 1;
    
    slotid_t sid = *sid_ptr; 
    pthread_mutex_unlock(&add_last_cb_sid_mutex);
    return sid;
}


pthread_mutex_t get_last_cb_sid_mutex = PTHREAD_MUTEX_INITIALIZER;
int get_last_cb_sid(groupid_t gid) {
    // TODO [FIX] need to lock.
    pthread_mutex_lock(&get_last_cb_sid_mutex);
    slotid_t* sid_ptr = apr_hash_get(lastslot_ht_, &gid, sizeof(gid));
    if (sid_ptr == NULL) {
        sid_ptr = apr_palloc(pl_global_, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(pl_global_, sizeof(groupid_t));
        *gid_ptr = gid;
        apr_hash_set(lastslot_ht_, gid_ptr, sizeof(gid), sid_ptr);
    }
    
    slotid_t sid = *sid_ptr; 
    pthread_mutex_unlock(&get_last_cb_sid_mutex);
    return sid;
}

int get_insnum(groupid_t gid, slotid_t** in) {
    slotid_t* sid_ptr = apr_hash_get(lastslot_ht_, &gid, sizeof(gid));
    if (sid_ptr == NULL) {
        sid_ptr = apr_palloc(pl_global_, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(pl_global_, sizeof(groupid_t));
        *gid_ptr = gid;
        apr_hash_set(lastslot_ht_, gid_ptr, sizeof(gid), sid_ptr);
    }
    *in = sid_ptr;
    return 0;
}

int get_instval(uint32_t gid, uint32_t in, char* buf,
        uint32_t bufsize, uint32_t *val_size) {
    //TODO [FIX]
    return 0;
}

bool has_value(groupid_t gid, slotid_t sid) {
    instid_t *p_iid = (instid_t *) calloc(1, sizeof(instid_t));
    p_iid->gid = gid;
    p_iid->sid = sid;

    value_t* v = apr_hash_get(val_ht_, p_iid, sizeof(instid_t));

    free(p_iid);
    return (v != NULL);
}

int put_instval(groupid_t gid, slotid_t sid, uint8_t *data,
        size_t sz_data) {
    pthread_mutex_lock(&value_mutex);
    
    instid_t *p_iid = (instid_t *) apr_pcalloc(pl_global_, sizeof(instid_t));
    p_iid->gid = gid;
    p_iid->sid = sid;
    value_t *val = (value_t *) apr_palloc(pl_global_, sizeof(value_t));
    val->data = (uint8_t *) apr_palloc(pl_global_, sz_data);
    val->len = sz_data;
    memcpy (val->data, data, sz_data);

    // TODO [IMPROVE] just put in memory now, should be put in bdb.
    apr_hash_set(val_ht_, p_iid, sizeof(instid_t), val);


    // TODO [IMPROVE] free some space in the map.
    // forget
    acceptor_forget();
    pthread_mutex_unlock(&value_mutex);
    return 0;
}
