#include "include_all.h"

static apr_pool_t *mp_txn_info_;
static apr_thread_mutex_t *mx_txn_info_; 
static apr_hash_t *ht_txn_info_;

void controller_init() {
    apr_pool_create(&mp_txn_info_, NULL);
    apr_thread_mutex_create(&mx_txn_info_, APR_THREAD_MUTEX_UNNESTED, mp_txn_info_); 
}

void controller_destroy() {
    apr_thread_mutex_destroy(mx_txn_info_);
}

void group_info_create(group_info_t **g, txn_info_t *tinfo, roundid_t *rid) {
    *g = (group_info_t*)malloc(sizeof(group_info_t));
    group_info_t *ginfo = *g;
    mpr_hash_create(&(ginfo->ht_prom));
    mpr_hash_create(&(ginfo->ht_accd));

    ginfo->gid = rid->gid;
    ginfo->sid = rid->sid;
    ginfo->bid = rid->bid;
    ginfo->tid = tinfo->tid;
    ginfo->n_promises_yes = 0;
    ginfo->n_promises_no = 0;
    ginfo->n_accepteds_yes = 0;
    ginfo->n_accepteds_no = 0;
    ginfo->max_prop = NULL;
}

void group_info_destroy(group_info_t *ginfo) {
    mpr_hash_destroy(ginfo->ht_prom);
    mpr_hash_destroy(ginfo->ht_accd);
    
    if (ginfo->max_prop != NULL) {
        prop_destroy(ginfo->max_prop);
    }
    free(ginfo);
}

void txn_info_create(txn_info_t **t, roundid_t **rids, size_t sz_rids, 
    mpaxos_req_t *req) {
    *t = (txn_info_t*) malloc(sizeof(txn_info_t));
    txn_info_t *info = *t;
    apr_pool_create(&info->mp, NULL);
    apr_thread_mutex_create(&info->mx, APR_THREAD_MUTEX_UNNESTED, info->mp);
    info->req = req;
    info->in_phase = 0;
    info->rids = rids;
    info->sz_rids = sz_rids;
    info->tid = req->id;
    info->ht_ginfo = apr_hash_make(info->mp);
    info->prop_max = NULL;
    info->prop_self = NULL;
}

void txn_info_destroy(txn_info_t *info) {
    for (int i = 0; i < info->sz_rids; i++) {
        free(info->rids[i]);
    }
    free(info->rids);
    apr_thread_mutex_destroy(info->mx);
    apr_hash_clear(info->ht_ginfo);
    apr_pool_destroy(info->mp);
    free(info);
}



txn_info_t *attach_txn_info(roundid_t **rids, size_t sz_rids, 
    mpaxos_req_t *req) {
    apr_thread_mutex_lock(mx_txn_info_);
    
    txn_info_t *info = apr_hash_get(ht_txn_info_, &(req->id), sizeof(roundid_t));
    // it should be a new round.
    SAFE_ASSERT(info == NULL);

    txn_info_create(&info, rids, sz_rids, req);
    
    for (size_t i = 0; i < sz_rids; i++) {
        group_info_t *ginfo = NULL;
        group_info_create(&ginfo, info, rids[i]);
        // add group to round
        apr_hash_set(info->ht_ginfo, &ginfo->gid, sizeof(groupid_t), ginfo);
    }
    // attach txn
    apr_hash_set(ht_txn_info_, &info->tid, sizeof(txnid_t), info);
    
    apr_thread_mutex_unlock(mx_txn_info_);
    return info;
}

void detach_txn_info(txn_info_t *tinfo) {
    apr_thread_mutex_lock(mx_txn_info_);
    apr_hash_set(ht_txn_info_, &tinfo->tid, sizeof(txnid_t), NULL);
    apr_hash_index_t *hi = NULL;
    for (hi = apr_hash_first(tinfo->mp, tinfo->ht_ginfo);
        hi; hi = apr_hash_next(hi)) {
        groupid_t *gid;
        group_info_t *ginfo = NULL;
        apr_hash_this(hi, (const void**)&gid, NULL, (void**)&ginfo);
        group_info_destroy(ginfo);
    }
    apr_hash_clear(tinfo->ht_ginfo);
    txn_info_destroy(tinfo);
    apr_thread_mutex_unlock(mx_txn_info_);
}

int mpaxos_start_request(mpaxos_req_t *req) {
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

        int is_me = 0;
        rids[i]->sid = get_newest_sid(req->gids[i], &is_me) + 1;
        if (!is_me) {
            is_saveprep = 0;
        }
    }
    
    int bid_base = (is_saveprep) ? BID_PRIOR : BID_NORMAL;
    for (int i = 0; i < req->sz_gids; i++) {
        rids[i]->bid = bid_base += req->n_retry;
    }

    txn_info_t *info = attach_txn_info(rids, req->sz_gids, req);
    
    if (is_saveprep) {
        LOG_DEBUG("save the prepare phase, go directly into phase 2.");
        info->in_phase = 2;
        mpaxos_accept(info);
    } else {
        info->in_phase =1;
        LOG_DEBUG("Start phase 1 prepare.");
        mpaxos_prepare(info);
    }
    return 0;
}


txn_info_t* get_txn_info(txnid_t tid) {
    apr_thread_mutex_lock(mx_txn_info_);
    txn_info_t *info = NULL;
    info = apr_hash_get(ht_txn_info_, &tid, sizeof(txnid_t));
    apr_thread_mutex_unlock(mx_txn_info_);
    return info;
}
