/**
 * change this to support only easy group node relationships. 
 * all nodes belong to all groups. you don't have to create groups.
 */

#include "view.h"
#include "comm.h"
#include "utils/logger.h"
#include "utils/safe_assert.h"
#include "utils/hostname.h"
#include "utils/mpr_hash.h"


static apr_pool_t *mp_view_ = NULL;
static apr_hash_t *gid_nid_ht_ht_; //groupid_t -> nodeid_t ht
static apr_hash_t *nid_gid_ht_ht_; //nodeid_t -> groupid_t ht
static mpr_hash_t *ht_node_info_ = NULL;    //nodename -> node_info
static nodeid_t local_nid_ = 0;
static char *nodename_ = NULL;

static apr_array_header_t *arr_nodes_ = NULL;
static bool init = false;

extern int port_;

void view_init() {
    apr_pool_create(&mp_view_, NULL);
    gid_nid_ht_ht_ = apr_hash_make(mp_view_);
    nid_gid_ht_ht_ = apr_hash_make(mp_view_);
    arr_nodes_ = apr_array_make(mp_view_, 10, sizeof(nodeid_t));
    mpr_hash_create(&ht_node_info_);
}

void view_destroy() {
    mpr_hash_destroy(ht_node_info_);
    apr_pool_destroy(mp_view_);
    if (nodename_ != NULL) {
        free(nodename_);
    }
}

void create_group(groupid_t gid) {
    // TODO [fix]
}

int get_group_size(groupid_t gid) {
    // TODO [fix]
/*
    gid = 1; // temporary
    apr_hash_t *nid_ht;
    nid_ht = apr_hash_get(gid_nid_ht_ht_, &gid, sizeof(gid));
    SAFE_ASSERT(nid_ht != NULL);
    int n = apr_hash_count(nid_ht);
    return n;
*/
    return arr_nodes_->nelts;
}

bool is_in_group(groupid_t gid) {
//    apr_hash_t *gid_ht;
//    gid_ht = apr_hash_get(nid_gid_ht_ht_, &local_nid_, sizeof(nodeid_t));
//    SAFE_ASSERT(gid_ht != NULL);
//    void *p = apr_hash_get(gid_ht, &gid, sizeof(nodeid_t));
//    return (p != NULL);
    return TRUE;
}

void set_node(const char *name, const char *addr, int port) {
    static nodeid_t nid = 0;
    nid++;
    char *ip = gethostip(addr);
    set_nid_sender(nid, ip, port);
    set_gid_nid(1, nid);
    
    node_info_t ninfo;
    ninfo.nid = nid;
    ninfo.port = port;
    SAFE_ASSERT(strlen(name) + 1 < 100);
    SAFE_ASSERT(strlen(ip) + 1 < 100);
    strcpy(ninfo.name, name);
    strcpy(ninfo.ip, ip);
    mpr_hash_set(ht_node_info_, name, strlen(name) + 1, &ninfo, sizeof(node_info_t));
    
    *(nodeid_t *)apr_array_push(arr_nodes_) = nid;
    LOG_INFO("node added. name:%s, id:%" PRIu64", addr: %s, port: %d", 
        name, nid, addr, port);
    free(ip);
}


void set_nodename(const char* nodename) {
    size_t sz = 0;
    node_info_t *ninfo = NULL;
    mpr_hash_get(ht_node_info_, nodename, strlen(nodename) + 1, (void**)&ninfo, &sz);
    SAFE_ASSERT(ninfo != NULL);
    local_nid_ = ninfo->nid;
    port_ = ninfo->port;
    nodename_ = malloc(strlen(nodename) + 1);
    strcpy(nodename_, nodename);
}

nodeid_t get_local_nid() {
    return local_nid_;
}


apr_array_header_t *get_group_nodes(groupid_t gid) {
    // [FIXME] different configurations for different groups
    return arr_nodes_;
}

/**
 * DEPRECATED
 */
apr_hash_t* view_group_table(groupid_t gid) {
    gid = 1; // temporary
    apr_hash_t *nid_ht = apr_hash_get(gid_nid_ht_ht_, &gid, sizeof(gid));
    return nid_ht;
}

int set_gid_nid(groupid_t gid, nodeid_t nid) {
    // need to save all the keys;
    groupid_t *gid_ptr = apr_palloc(mp_view_, sizeof(groupid_t));
    nodeid_t *nid_ptr = apr_palloc(mp_view_, sizeof(nodeid_t));
    *gid_ptr = gid;
    *nid_ptr = nid;

    apr_hash_t *nid_ht;
    apr_hash_t *gid_ht;

    nid_ht = apr_hash_get(gid_nid_ht_ht_, gid_ptr, sizeof(gid));
    if (nid_ht == NULL) {
     nid_ht =   apr_hash_make(mp_view_);
     apr_hash_set(gid_nid_ht_ht_, gid_ptr, sizeof(gid), nid_ht);
    }
    apr_hash_set(nid_ht, nid_ptr, sizeof(nid), nid_ptr);
    
    gid_ht = apr_hash_get(nid_gid_ht_ht_, nid_ptr, sizeof(nid));
    if (gid_ht == NULL) {
        gid_ht = apr_hash_make(mp_view_);
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
/*
    *sz_gids = apr_hash_count(gid_nid_ht_ht_);
    *gids = malloc(*sz_gids * sizeof(groupid_t));
    apr_hash_index_t *hi;
    int i = 0;
    for (hi = apr_hash_first(mp_view_, gid_nid_ht_ht_); hi; hi = apr_hash_next(hi)) {
        groupid_t *k;
        apr_hash_this(hi, (const void**)&k, NULL, NULL);
        (*gids)[i] = *k;
        i++;
    }
*/
}
