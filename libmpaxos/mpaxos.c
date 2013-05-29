
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

#define QUEUE_SIZE 1000

// TODO [FIX] a lot of problems
#define MAX_THREADS 1


apr_pool_t *pool_ptr;
apr_hash_t *val_ht_;        //instid_t -> value_t
apr_hash_t *lastslot_ht_;   //groupid_t -> slotid_t #hash table to store the last slot id that has been called back.
apr_hash_t *cb_ht_;         //groupid_t -> mpaxos_cb_t
apr_hash_t *cb_req_ht_;    //instid_t -> cb_para #not implemented in this version. 
apr_hash_t *cb_me_ht_;      // groupid_t -> pthread_mutex_t 

apr_queue_t *request_q_;

pthread_t t_commit_async_;

apr_thread_pool_t *p_thread_pool_;  // thread pool for asynchrous commit and call back.

uint32_t n_subthread_ = 0;  // count of asynchrous threads.
pthread_mutex_t n_subthreads_mutex; //mutex lock for above   

// perhaps we need a large hashtable, probably not
// groupid_t -> some structure
// some structure:
//      a hash table to store value sid->value   ###need to be permanent         
//      last slotid_t that has been called back. ###need to be permanent
//      callback function.      ###cannot be be permanent 
//      a hash table to store callback parameter sid->cb_para    ###permanent?  now not.
//      a mutex for callback. 

mpaxos_cb_t *cb_god_ = NULL;

pthread_mutex_t value_mutex;

int port_;

void mpaxos_init() {
    apr_initialize();
    apr_pool_create(&pool_ptr, NULL);
    lastslot_ht_ = apr_hash_make(pool_ptr);
    val_ht_ = apr_hash_make(pool_ptr);
    cb_ht_ = apr_hash_make(pool_ptr);
    cb_me_ht_ = apr_hash_make(pool_ptr);
    cb_req_ht_ = apr_hash_make(pool_ptr);

    apr_queue_create(&request_q_, QUEUE_SIZE, pool_ptr);

    apr_thread_pool_create(&p_thread_pool_, MAX_THREADS, MAX_THREADS, pool_ptr);

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

    //start_server(port);

    pthread_mutex_init(&value_mutex, NULL);
}

void mpaxos_start() {
    // start listening
    start_server(port_);
    
    // start the background thread for asynchrous commit. 
    pthread_create(&t_commit_async_, NULL, (void* (*)(void*))(commit_async_run), NULL); 
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

    // TODO [IMPROVE] stop asynchrouns callback.

    apr_pool_destroy(pool_ptr);
    apr_terminate();

    pthread_mutex_destroy(&value_mutex);
}

void mpaxos_set_cb(groupid_t gid, mpaxos_cb_t cb) {
    groupid_t *k = apr_palloc(pool_ptr, sizeof(gid));
    mpaxos_cb_t *v = apr_palloc(pool_ptr, sizeof(cb));
    *k = gid;
    *v = cb;
    apr_hash_set(cb_ht_, k, sizeof(gid), v);

    // initialize mutex for call back.
    pthread_mutex_t *p_me = apr_palloc(pool_ptr, sizeof(pthread_mutex_t));
    SAFE_ASSERT(p_me != NULL);
    pthread_mutex_init(p_me, NULL);
    apr_hash_set(cb_me_ht_, k, sizeof(groupid_t), p_me); 
}

void mpaxos_set_cb_god(mpaxos_cb_t cb) {
    cb_god_ = cb;
}

void lock_callback(groupid_t gid) {
    char buf[100];
    sprintf(buf, "CALLBACK%x", gid);
    m_lock(buf);
}

void unlock_callback(groupid_t gid) {
    char buf[100];
    sprintf(buf, "CALLBACK%x", gid);
    m_unlock(buf);
}

// !!!IMPORTANT!!!
// this function is NOT completely thread-safe.
// its track should be like this. 
//      1. there is a background thread keeping checking a queue of asynchrous requests. 
//      2. whenever it finds out there is a new request, it runs this request in one thread in pool.
//      3. in each sub-thread, invoke_callback is called. every two subthreads must not push the same gids, so lock is needed. 
//      4. TODO [IMPROVE] one thread can loop do callback, other thread return. this is tricky because no tails should be left.
void invoke_callback(groupid_t gid, mpaxos_request_t* p_r) {
    // get the callback function
    LOG_DEBUG("invoke callback on group %d", gid);

    lock_callback(gid);
    mpaxos_cb_t *cb = (cb_god_ != NULL) ? &cb_god_ : apr_hash_get(cb_ht_, &gid, sizeof(gid));

    if (cb == NULL) {
        LOG_WARN("no callback on group: %d", gid);
        return;
    }
    
    // lock gid here.

    // check call history, and go forward. 
    // if there is value at sid + 1, then callback!
    slotid_t sid = 0;
    int c = 0;
    while (has_value(gid, (sid = get_last_cb_sid(gid) + 1)) && ++c) {
        LOG_DEBUG("prepare to callback on group %d", gid);
        // get the call back data and para.
        SAFE_ASSERT(p_r != NULL);
        // callback in current thread.

        (*cb)(gid, sid, p_r->data, p_r->sz_data, p_r->cb_para);

        // callback finished, push the called count.
        add_last_cb_sid(gid); 
    }
    LOG_DEBUG("%d callback invoked on group %d, last callback: %d", c, gid, sid-1);
    unlock_callback(gid);
}

void set_listen_port(int port) {
    port_ = port;
}

int commit_sync(groupid_t* gids, size_t gid_len, uint8_t *val,
        size_t val_len) {

    // find the appropriate proposer
    // keep propose until success
    LOG_DEBUG("Try to commit.");
    struct timespec ts_begin;
    struct timespec ts_end;

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
        get_realtime(&ts_begin);

        int rids_len = gid_len;
        int ret = run_full_round(gid, 1, rids_ptr, rids_len, val, val_len, 1000000);
        if (ret == 0) {
            get_realtime(&ts_end);
            long millisec = (ts_end.tv_sec - ts_begin.tv_sec) * 1000 + (ts_end.tv_nsec - ts_begin.tv_nsec) / 1000 / 1000;
            if (millisec > 100) {
                LOG_WARN("Value committed. Cost time too long: %d", millisec);
            } else {
                LOG_DEBUG("Value committed. Cost time: %d", millisec);
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

void* APR_THREAD_FUNC commit_async_job(apr_thread_t *p_t, void *v) {
    // cannot call on same group concurrently, otherwise would be wrong.
    LOG_DEBUG("commit in a ASYNC job");
    mpaxos_req_t *p_r = v;
    lock_group_commit(p_r->gids, p_r->sz_gids);
    commit_sync(p_r->gids, p_r->sz_gids, p_r->data, p_r->sz_data); 
     // invoke callback.
    for (int i = 0; i < p_r->sz_gids; i++) {
        groupid_t gid = p_r->gids[i];
        invoke_callback(gid, p_r);
    }
    unlock_group_commit(p_r->gids, p_r->sz_gids);
    // free p_r 
    free(p_r->data);
    free(p_r->gids);
    free(p_r);
    return NULL;
}

/**
 * this runs in a background thread. keeps poping from the async job queue.
 * give the job the a thread in the thread pool.
 */
int commit_async_run() {
    // suppose the thread is here
    LOG_DEBUG("the ASYNC running threading started");
    while (1) {
        // in a loop
        // check whether the pool or queue is empty?
        mpaxos_request_t *p_r = NULL;
        apr_queue_pop(request_q_, (void**)&p_r); // This will block if queue is empty
        SAFE_ASSERT(p_r != NULL);

        LOG_DEBUG("ASYNC request poped");
        // we need a thread pool here. for now we just run it here in this thread.
        // each group may have more than one subthread. but need to be more careful later.
        apr_thread_pool_push(p_thread_pool_, commit_async_job, (void*)p_r, 0, NULL);

        // when to free p_r? anyway, this is a bad timing to free. 
        //free(p_r->gids);
        //free(p_r->data);
        //free(p_r);
    }
    return 0;
}

/**
 * commit a request that is to be processed asynchronously. add the request to the aync job queue. 
 */
int commit_async(groupid_t* gids, size_t gid_len, uint8_t *val,
        size_t val_len, void* cb_para) {
    // copy the request.
    LOG_DEBUG("commit_async called");
    mpaxos_request_t *p_r = (mpaxos_request_t *)malloc(sizeof(mpaxos_request_t));
    p_r->sz_gids = gid_len;
    p_r->gids = malloc(gid_len * sizeof(groupid_t));
    p_r->sz_data = val_len;
    p_r->data = malloc(val_len);
    p_r->cb_para = cb_para;
    memcpy(p_r->gids, gids, gid_len * sizeof(groupid_t));
    memcpy(p_r->data, val, val_len);

    // push the request to the queue.
    apr_queue_push(request_q_, p_r);

    // TODO [IMPROVE] if the thread is sleeping, wake up the thread. currently the thread is just blocking.
    return 0;
}

pthread_mutex_t add_last_cb_sid_mutex = PTHREAD_MUTEX_INITIALIZER;
int add_last_cb_sid(groupid_t gid) {
    pthread_mutex_lock(&add_last_cb_sid_mutex);
    slotid_t* sid_ptr = apr_hash_get(lastslot_ht_, &gid, sizeof(gid));
    if (sid_ptr == NULL) {
        sid_ptr = apr_palloc(pool_ptr, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(pool_ptr, sizeof(groupid_t));
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
        sid_ptr = apr_palloc(pool_ptr, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(pool_ptr, sizeof(groupid_t));
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
        sid_ptr = apr_palloc(pool_ptr, sizeof(slotid_t));
        *sid_ptr = 0;
        groupid_t *gid_ptr = apr_palloc(pool_ptr, sizeof(groupid_t));
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
    
    instid_t *p_iid = (instid_t *) calloc(1, sizeof(instid_t));
    p_iid->gid = gid;
    p_iid->sid = sid;
    value_t *val = (value_t *) malloc(sizeof(value_t));
    val->data = (uint8_t *) malloc(sz_data);
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
