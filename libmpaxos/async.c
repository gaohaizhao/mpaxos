
/**
 * Take the unthoughtful approach, each group
 * has a request queue and a lock. The deamon
 * keeps scanning every queue with the lock.
 */
#define MAX_THREADS 20

#include <pthread.h>
#include <apr_atomic.h>
#include <apr_time.h>
#include <apr_hash.h>
#include <apr_ring.h>
#include <apr_thread_proc.h>
#include <apr_thread_pool.h>
#include "mpaxos/mpaxos.h"
#include "async.h"
#include "view.h"
#include "utils/logger.h"
#include "utils/safe_assert.h"


static apr_pool_t *pl_async_; // a pool for constants in asynchrounous callback module.
static mpaxos_cb_t cb_god_ = NULL; // a god callback function on all groups.
static apr_thread_pool_t *p_thread_pool_;  // thread pool for asynchrous commit and call back.
static apr_thread_mutex_t *me_gids_; // a lock to help visit the three hash tables below.
static apr_hash_t *ht_me_cb_;   // groupid_t -> apr_thread_mutex_t, lock on each group for callback.
static apr_hash_t *ht_qu_cb_;   // groupid_t -> apr_ring_t, shit! finally I need a lot of queues to do this.
static apr_hash_t *ht_me_ri_;   // groupid_t -> apr_thread_mutex_t, lock on each group ring.
// static apr_thread_mutex_t *me_daemon_;
// static apr_thread_cond_t *cd_daemon_;
//static pthread_t th_daemon_; // daemon thread
static apr_thread_t *th_daemon_; // daemon thread


apr_uint32_t n_req_ = 0;
apr_uint32_t is_exit_ = 0;


typedef struct _req_elem_t {
    APR_RING_ENTRY(_req_elem_t) link;
    mpaxos_req_t *req;
} req_elem_t;
typedef struct _req_ring_t req_ring_t;
APR_RING_HEAD(_req_ring_t, _req_elem_t);


// unused now.
static apr_hash_t *cb_ht_;         //groupid_t -> mpaxos_cb_t
static apr_hash_t *cb_req_ht_;    //instid_t -> cb_para #not implemented in this version. 
static uint32_t n_subthread_ = 0;  // count of asynchrous threads.
static pthread_mutex_t n_subthreads_mutex;  //mutex lock for above   


void mpaxos_async_init() {
    apr_pool_create(&pl_async_, NULL);
    apr_thread_pool_create(&p_thread_pool_, MAX_THREADS, MAX_THREADS, pl_global_);
    apr_thread_mutex_create(&me_gids_, APR_THREAD_MUTEX_UNNESTED, pl_async_);
    ht_me_cb_ = apr_hash_make(pl_async_);
    ht_me_ri_ = apr_hash_make(pl_async_);
    ht_qu_cb_ = apr_hash_make(pl_async_);

    // insert all groups into the two hash tables.
    groupid_t *gids;
    size_t sz_gids;

    // start the background daemon for asynchrous commit. 
    apr_thread_create(&th_daemon_, NULL, mpaxos_async_daemon, NULL, pl_async_); 

    // unused now.
    //cb_ht_ = apr_hash_make(pool_ptr);
    //cb_req_ht_ = apr_hash_make(pool_ptr);
    //apr_queue_create(&request_q_, QUEUE_SIZE, pool_ptr);
}

req_ring_t* get_group_ring(groupid_t gid) {
    apr_thread_mutex_lock(me_gids_);
    req_ring_t *r = apr_hash_get(ht_qu_cb_, &gid, sizeof(groupid_t));
    if (r == NULL) {
        r = apr_palloc(pl_async_, sizeof(req_ring_t));
        APR_RING_INIT(r, _req_elem_t, link);
        groupid_t *g = apr_palloc(pl_async_, sizeof(groupid_t));
        *g = gid;
        apr_hash_set(ht_qu_cb_, g, sizeof(groupid_t), r);
    }
    apr_thread_mutex_unlock(me_gids_);
    return r;
}
/** 
 * try lock multiple groups atomically
 * this is called before a request is pushed into the thread pool. ?????
 */
bool try_lock_groups_callback(groupid_t *gids, size_t sz_gids) {
    bool ret = true;
    apr_thread_mutex_lock(me_gids_);
    for (size_t i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        apr_thread_mutex_t *m = apr_hash_get(ht_me_cb_, &gid, sizeof(groupid_t));
        if (m == NULL) {
            apr_thread_mutex_create(&m, APR_THREAD_MUTEX_UNNESTED, pl_async_);
            groupid_t *g = apr_palloc(pl_async_, sizeof(groupid_t));
            *g = gid;
            apr_hash_set(ht_me_cb_, g, sizeof(groupid_t), m);
        }
        apr_status_t s = apr_thread_mutex_trylock(m);
        if (APR_STATUS_IS_EBUSY(s)) {
            // roll back all the lock
            for (size_t j = 0; j < i; j++) {
                groupid_t gid = gids[i];
                apr_thread_mutex_t *m = apr_hash_get(ht_me_cb_, &gid, sizeof(groupid_t));
                apr_thread_mutex_unlock(m);
            }
            ret = false;
        }
    }
    apr_thread_mutex_unlock(me_gids_);
    return ret;
}

void unlock_groups_callback(groupid_t *gids, size_t sz_gids) {
    apr_thread_mutex_lock(me_gids_);
    for (size_t i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        apr_thread_mutex_t *m = apr_hash_get(ht_me_cb_, &gid, sizeof(groupid_t));
        apr_thread_mutex_unlock(m);
    }
    apr_thread_mutex_unlock(me_gids_);
}

void lock_groups_ring(groupid_t *gids, size_t sz_gids) {
    apr_thread_mutex_lock(me_gids_);
    for (size_t i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        apr_thread_mutex_t *m = apr_hash_get(ht_me_ri_, &gid, sizeof(groupid_t));
        if (m == NULL) {
            apr_thread_mutex_create(&m, APR_THREAD_MUTEX_UNNESTED, pl_async_);
            groupid_t *g = apr_palloc(pl_async_, sizeof(groupid_t));
            *g = gid;
            apr_hash_set(ht_me_ri_, g, sizeof(groupid_t), m);
        }
        apr_thread_mutex_lock(m);
    }
    apr_thread_mutex_unlock(me_gids_);
}

void unlock_groups_ring(groupid_t *gids, size_t sz_gids) {
    apr_thread_mutex_lock(me_gids_);
    for (size_t i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        apr_thread_mutex_t *m = apr_hash_get(ht_me_ri_, &gid, sizeof(groupid_t));
        apr_thread_mutex_unlock(m);
    }
    apr_thread_mutex_unlock(me_gids_);
}

void pop_groups(groupid_t *gids, size_t sz_gids) {
    lock_groups_ring(gids, sz_gids);
    for (size_t i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        req_ring_t *r = apr_hash_get(ht_qu_cb_, &gid, sizeof(groupid_t));
        req_elem_t *e = APR_RING_FIRST(r);
        APR_RING_UNSPLICE(e, e, link);
        free(e);
    }
    unlock_groups_ring(gids, sz_gids);
}

mpaxos_req_t *glance_group(groupid_t gid) {
    mpaxos_req_t *req;
    lock_groups_ring(&gid, 1);
    req_ring_t *r = get_group_ring(gid);
    req_elem_t *e = APR_RING_FIRST(r);
    if (e == APR_RING_SENTINEL(r, _req_elem_t, link)) {
        req = NULL;
    } else {
        req = e->req; 
    }
    unlock_groups_ring(&gid, 1);
    return req;
}

void mpaxos_async_enlist(groupid_t *gids, size_t sz_gids, uint8_t *data, size_t sz_data, void* cb_para) {
    mpaxos_req_t *p_r = (mpaxos_req_t *)malloc(sizeof(mpaxos_req_t));
    p_r->sz_gids = sz_gids;
    p_r->gids = malloc(sz_gids * sizeof(groupid_t));
    p_r->sz_data = sz_data;
    p_r->data = malloc(sz_data);
    p_r->cb_para = cb_para;
    memcpy(p_r->gids, gids, sz_gids * sizeof(groupid_t));
    memcpy(p_r->data, data, sz_data);

    // push the request to the ring
    lock_groups_ring(gids, sz_gids);
    for (size_t i = 0; i < sz_gids; i++) {
        groupid_t gid = gids[i];
        req_elem_t *obj = malloc(sizeof(req_elem_t));
        obj->req = p_r;
        req_ring_t *r = get_group_ring(gid); 
        APR_RING_INSERT_TAIL(r, obj, _req_elem_t, link);
    }
    unlock_groups_ring(gids, sz_gids);
    apr_atomic_inc32(&n_req_);
    LOG_DEBUG("request %d enlisted.", apr_atomic_read32(&n_req_));
    // TODO [IMPROVE]
    // invoke deamon to check on the ring.
}


bool glance_groups_equal(groupid_t *gids, size_t sz_gids) {
    //lock_groups_ring(gids, sz_gids);
    mpaxos_req_t *r = NULL;
    for (size_t i = 0; i < sz_gids; i++) {
        groupid_t g_tmp = gids[i];
        mpaxos_req_t *p_r_tmp = glance_group(g_tmp);
        if (r == NULL) {
            r = p_r_tmp;
        } else if (p_r_tmp != r) {
            return false;
        }
    }
    return r != NULL;
    //unlock_groups_ring(gids, sz_gids);
}

/**
 * !!!IMPORTANT!!!
 * this function is NOT entirely thread-safe. same gid cannot call concurrently.
 */
void invoke_callback(groupid_t gid, mpaxos_request_t* p_r) {
    // get the callback function
    LOG_DEBUG("invoke callback on group %d", gid);

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
}

void mpaxos_set_cb(groupid_t gid, mpaxos_cb_t cb) {
    //groupid_t *k = apr_palloc(pool_ptr, sizeof(gid));
    //mpaxos_cb_t *v = apr_palloc(pool_ptr, sizeof(cb));
    //*k = gid;
    //*v = cb;
    //apr_hash_set(cb_ht_, k, sizeof(gid), v);

    //// initialize mutex for call back.
    //pthread_mutex_t *p_me = apr_palloc(pool_ptr, sizeof(pthread_mutex_t));
    //SAFE_ASSERT(p_me != NULL);
    //pthread_mutex_init(p_me, NULL);
    //apr_hash_set(cb_me_ht_, k, sizeof(groupid_t), p_me); 
}

void mpaxos_set_cb_god(mpaxos_cb_t cb) {
    cb_god_ = cb;
}

void mpaxos_async_destroy() {
    // TODO [improve] recycle everything.
    
    apr_atomic_set32(&is_exit_, 1);
    apr_status_t s;
    apr_thread_join(&s, th_daemon_);
    apr_pool_destroy(pl_async_);
}

void* APR_THREAD_FUNC commit_async_job(apr_thread_t *p_t, void *v) {
    // cannot call on same group concurrently, otherwise would be wrong.
    LOG_DEBUG("commit in a ASYNC job");
    mpaxos_req_t *p_r = v;
    commit_sync(p_r->gids, p_r->sz_gids, p_r->data, p_r->sz_data); 
     // invoke callback.
    for (int i = 0; i < p_r->sz_gids; i++) {
        groupid_t gid = p_r->gids[i];
        invoke_callback(gid, p_r);
    }

    // unlock group
    unlock_groups_callback(p_r->gids, p_r->sz_gids);
    
    // TODO [improve] 
    // invoke on the daemon to rerun the test.

    // free p_r 
    free(p_r->data);
    free(p_r->gids);
    free(p_r);

    return NULL;
}

void mpaxos_async_daemon_scan(groupid_t g) {
    do {
        mpaxos_request_t *p_r = glance_group(g);
    
        if (p_r == NULL) break;

        if (!glance_groups_equal(p_r->gids, p_r->sz_gids)) break;

        if (!try_lock_groups_callback(p_r->gids, p_r->sz_gids)) break;

        pop_groups(p_r->gids, p_r->sz_gids);

        // invoke callback
        LOG_DEBUG("ASYNC request poped");
        // we need a thread pool here. for now we just run it here in this thread.
        // each group may have more than one subthread. but need to be more careful later.
        apr_thread_pool_push(p_thread_pool_, commit_async_job, (void*)p_r, 0, NULL);
        // when to free p_r? anyway, this is a bad timing to free. 
        
    } while (0);
}


/**
 * this runs in a background thread. keeps poping from the async job queue.
 * give the job the a thread in the thread pool.
 * its track should be like this. 
 *      1. background daemon thread keeps checking queues of asynchrous requests. 
 *      2. whenever it finds out there is a new request, it runs this request in one thread in pool.
 *      3. in each sub-thread, invoke_callback is called. 
 *      4. TODO [IMPROVE] one thread can loop do callback, other thread return. this is tricky because no tails should be left.
 */
void* APR_THREAD_FUNC mpaxos_async_daemon(apr_thread_t *th, void* data) {
    // suppose the thread is here
    LOG_DEBUG("async daemon thread start");
    while (!apr_atomic_read32(&is_exit_)) {
        // in a loop
        // check whether the pool or queue is empty?

        // use get all groups instead of iterating the hash table.
        // scan the hash table for queues.
        groupid_t *gids;
        size_t sz_gids;
        get_all_groupids(&gids, &sz_gids); // this is in view.c

        for (size_t i = 0; i < sz_gids && !apr_atomic_read32(&is_exit_); i++) {
            groupid_t gid = gids[i];
            mpaxos_async_daemon_scan(gid); 
        }
        free(gids);

        // apr_thread_mutex_unlock(me_gids_);
        apr_sleep(10000);
    }
    LOG_DEBUG("async daemon thread exit");
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}
