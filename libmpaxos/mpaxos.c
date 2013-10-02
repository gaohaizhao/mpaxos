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
#include "recorder.h"

#define QUEUE_SIZE 1000

// TODO [FIX] a lot of problems


apr_pool_t *mp_global_;
apr_hash_t *lastslot_ht_;   //groupid_t -> slotid_t #hash table to store the last slot id that has been called back.

// perhaps we need a large hashtable, probably not
// groupid_t -> some structure
// some structure:
//      a hash table to store value sid->value   ###need to be permanent         
//      last slotid_t that has been called back. ###need to be permanent
//      callback function.      ###cannot be be permanent 
//      a hash table to store callback parameter sid->cb_para    ###permanent?  now not.
//      a mutex for callback. 

int port_;

void mpaxos_init() {
    apr_initialize();
    apr_pool_create(&mp_global_, NULL);
    lastslot_ht_ = apr_hash_make(mp_global_);

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
    
    // initialize the recorder
    recorder_init();
    
    // initialize asynchrounous commit and callback module
    mpaxos_async_init(); 

    
    //start_server(port);

}

void mpaxos_start() {
    // start listening
    start_server(port_);
}

void mpaxos_stop() {
    // TODO [improve] when to stop?
}

void mpaxos_destroy() {
    comm_destroy();
    
    acceptor_destroy();
    proposer_final();

    // stop asynchrouns callback.
    mpaxos_async_destroy();
    apr_pool_destroy(mp_global_);
    apr_terminate();

}

void set_listen_port(int port) {
    port_ = port;
}

int commit_sync(groupid_t* gids, size_t gid_len, uint8_t *val,
        size_t val_len) {
	// TODO
    return 0;
}

void lock_group_commit(groupid_t* gids, size_t sz_gids) {
    char buf[100];
    for (int i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        sprintf(buf, "COMMIT%lx", gid);
        m_lock(buf);
    }
}

void unlock_group_commit(groupid_t* gids, size_t sz_gids) {
    char buf[100];
    for (int i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        sprintf(buf, "COMMIT%lx", gid);
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
        sid_ptr = apr_palloc(mp_global_, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(mp_global_, sizeof(groupid_t));
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
        sid_ptr = apr_palloc(mp_global_, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(mp_global_, sizeof(groupid_t));
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
        sid_ptr = apr_palloc(mp_global_, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(mp_global_, sizeof(groupid_t));
        *gid_ptr = gid;
        apr_hash_set(lastslot_ht_, gid_ptr, sizeof(gid), sid_ptr);
    }
    *in = sid_ptr;
    return 0;
}


