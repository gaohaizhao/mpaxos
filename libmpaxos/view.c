/**
 * change this to support only easy group node relationships. all nodes belong to all groups. you don't have to create groups.
 */

#include "view.h"
#include "utils/safe_assert.h"

apr_pool_t *view_pool;
apr_hash_t *gid_nid_ht_ht_; //groupid_t -> nodeid_t ht
apr_hash_t *nid_gid_ht_ht_; //nodeid_t -> groupid_t ht

nodeid_t local_nid_;

bool init = false;

void view_init() {
    apr_pool_create(&view_pool, NULL);
    gid_nid_ht_ht_ = apr_hash_make(view_pool);
    nid_gid_ht_ht_ = apr_hash_make(view_pool);
}

void view_final() {
    apr_pool_destroy(view_pool);
}

void create_group(groupid_t gid) {
    // TODO [emergent]
}

int get_group_size(groupid_t gid) {
    gid = 1; // temporary
    apr_hash_t *nid_ht;
    nid_ht = apr_hash_get(gid_nid_ht_ht_, &gid, sizeof(gid));
    SAFE_ASSERT(nid_ht != NULL);
    int n = apr_hash_count(nid_ht);
    return n;
}

bool is_in_group(groupid_t gid) {
//    apr_hash_t *gid_ht;
//    gid_ht = apr_hash_get(nid_gid_ht_ht_, &local_nid_, sizeof(nodeid_t));
//    SAFE_ASSERT(gid_ht != NULL);
//    void *p = apr_hash_get(gid_ht, &gid, sizeof(nodeid_t));
//    return (p != NULL);
    return TRUE;
}

void set_local_nid(nodeid_t nid) {
    local_nid_ = nid;
}

nodeid_t get_local_nid() {
    return local_nid_;
}

apr_hash_t* view_group_table(groupid_t gid) {
    gid = 1; // temporary
    apr_hash_t *nid_ht = apr_hash_get(gid_nid_ht_ht_, &gid, sizeof(gid));
    return nid_ht;
}

int set_gid_nid(groupid_t gid, nodeid_t nid) {
    // need to save all the keys;
    groupid_t *gid_ptr = apr_palloc(view_pool, sizeof(groupid_t));
    nodeid_t *nid_ptr = apr_palloc(view_pool, sizeof(nodeid_t));
    *gid_ptr = gid;
    *nid_ptr = nid;

    apr_hash_t *nid_ht;
    apr_hash_t *gid_ht;

    nid_ht = apr_hash_get(gid_nid_ht_ht_, gid_ptr, sizeof(gid));
    if (nid_ht == NULL) {
     nid_ht =   apr_hash_make(view_pool);
     apr_hash_set(gid_nid_ht_ht_, gid_ptr, sizeof(gid), nid_ht);
    }
    apr_hash_set(nid_ht, nid_ptr, sizeof(nid), nid_ptr);
    
    gid_ht = apr_hash_get(nid_gid_ht_ht_, nid_ptr, sizeof(nid));
    if (gid_ht == NULL) {
        gid_ht = apr_hash_make(view_pool);
        apr_hash_set(nid_gid_ht_ht_, nid_ptr, sizeof(nid), gid_ht);
    }
    apr_hash_set(gid_ht, gid_ptr, sizeof(gid), gid_ptr);

    return 0;
}

/**
 * Be careful with this function, it might take time because there may be too many groups.
 * @param gids
 * @param sz_gids
 */
void get_all_groupids(groupid_t **gids, size_t *sz_gids) {
    *sz_gids = apr_hash_count(gid_nid_ht_ht_);
    *gids = malloc(*sz_gids * sizeof(groupid_t));
    apr_hash_index_t *hi;
    int i = 0;
    for (hi = apr_hash_first(view_pool, gid_nid_ht_ht_); hi; hi = apr_hash_next(hi)) {
        groupid_t *k;
        apr_hash_this(hi, (const void**)&k, NULL, NULL);
        (*gids)[i] = *k;
        i++;
    }
}
