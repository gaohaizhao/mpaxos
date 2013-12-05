
#include "include_all.h"

static apr_pool_t *mp_comm_;
static apr_hash_t *ht_sender_; //nodeid_t -> sender_t
static apr_thread_mutex_t *mx_comm_;

server_t *server_ = NULL;

//View

//uint64_t recv_data_count = 0;
//uint64_t recv_msg_count = 0;
//time_t recv_start_time = 0;
//time_t recv_last_time = 0;
//time_t recv_curr_time = 0;

void comm_init() {
    apr_pool_create(&mp_comm_, NULL);
    ht_sender_ = apr_hash_make(mp_comm_);
    apr_thread_mutex_create(&mx_comm_, APR_THREAD_MUTEX_UNNESTED, mp_comm_);
    rpc_init();
}

void comm_destroy() {
    rpc_destroy();
    LOG_DEBUG("stopped listening on network.");

    // destroy all senders and recvrs
    apr_array_header_t *arr_nid = get_view(1);
    SAFE_ASSERT(arr_nid != NULL);
    for (int i = 0; i < arr_nid->nelts; i++) {
        nodeid_t nid = arr_nid->elts[i];    
        client_t *c = NULL;
        c = apr_hash_get(ht_sender_, &nid, sizeof(nodeid_t));
        client_destroy(c);
    }

    if (server_) {
        server_destroy(server_);
    }

    apr_thread_mutex_destroy(mx_comm_);
    apr_pool_destroy(mp_comm_);
}

void send_to(nodeid_t nid, msg_type_t type, const uint8_t *data,
    size_t sz) {
    client_t *s_ptr;
    apr_thread_mutex_lock(mx_comm_);
    s_ptr = apr_hash_get(ht_sender_, &nid, sizeof(nid));
    apr_thread_mutex_unlock(mx_comm_);
    //int hash_size = apr_hash_count(sender_ht_);

    SAFE_ASSERT(s_ptr != NULL);
    client_call(s_ptr, type, data, sz);
}

slotid_t send_to_slot_mgr(groupid_t gid, nodeid_t nid, uint8_t *data,
        size_t sz) {
/*
    sender_t *s_ptr;
    s_ptr = apr_hash_get(ht_sender_, &nid, sizeof(nid));

    sender_t *s = (sender_t *)malloc(sizeof(sender_t));
    strcpy(s->addr, s_ptr->addr);
    s->port = s_ptr->port;
    sender_init(s);

    uint8_t* buf = (uint8_t*) malloc(100);
    mpaxos_send_recv(s, data, sz, buf, 100); 
    slotid_t sid = (slotid_t)*buf;
    free(buf);
    free(s);
    return sid; 
*/
}
/**
 * thread safe.
 * @param gid
 * @param type
 * @param buf
 * @param sz
 */
void send_to_group(groupid_t gid, msg_type_t type, const uint8_t *buf,
    size_t sz) {
	// TODO [FIX] this is not thread safe because of apache hash table
//  apr_hash_t *nid_ht = apr_hash_get(gid_nid_ht_ht_, &gid, sizeof(gid));
    apr_array_header_t *arr_nid = get_view(gid);
    SAFE_ASSERT(arr_nid != NULL);

    for (int i = 0; i < arr_nid->nelts; i++) {
        nodeid_t nid = arr_nid->elts[i];
        send_to(nid, type, buf, sz);
    }
}

void connect_all_senders() {
    // [FIXME]
    apr_array_header_t *arr_nid = get_view(1);
    SAFE_ASSERT(arr_nid != NULL);

    for (int i = 0; i < arr_nid->nelts; i++) {
        nodeid_t nid = arr_nid->elts[i];
        client_t *c = NULL;
        c = apr_hash_get(ht_sender_, &nid, sizeof(nodeid_t));
        client_connect(c);
    }
}

void send_to_groups(groupid_t* gids, size_t sz_gids,
        msg_type_t type, const char *buf, size_t sz) {
    groupid_t gid = gids[0];
    send_to_group(gid, type, (const uint8_t *)buf, sz);
    // TODO [IMPROVE] Optimize
//    for (uint32_t i = 0; i < sz_gids; i++, gids++) {
//        gid = *gids;
//        send_to_group(gid, (const uint8_t *)buf, sz);
//    }
}

rpc_state* on_prepare(rpc_state* state) {
    msg_prepare_t *msg_prep;
    msg_prep = mpaxos__msg_prepare__unpack(NULL, state->sz, state->buf);
    log_message_rid("receive", "PREPARE", msg_prep->h, msg_prep->rids, 
            msg_prep->n_rids, state->sz);
    rpc_state* ret_state = handle_msg_prepare(msg_prep);
    mpaxos__msg_prepare__free_unpacked(msg_prep, NULL);
    return ret_state;
}

rpc_state* on_accept(rpc_state* state) {
    msg_accept_t *msg_accp;
    msg_accp = mpaxos__msg_accept__unpack(NULL, state->sz, state->buf);
    log_message_rid("receive", "ACCEPT", msg_accp->h, msg_accp->prop->rids,
            msg_accp->prop->n_rids, state->sz);
    rpc_state* ret_state = handle_msg_accept(msg_accp);
    mpaxos__msg_accept__free_unpacked(msg_accp, NULL);
    return ret_state;
}

rpc_state* on_promise(rpc_state* state) {
    msg_promise_t *msg_prom;
    msg_prom = mpaxos__msg_promise__unpack(NULL, state->sz, state->buf);
    log_message_res("receive", "PROMISE", msg_prom->h, msg_prom->ress, 
            msg_prom->n_ress, state->sz);
    handle_msg_promise(msg_prom);
    mpaxos__msg_promise__free_unpacked(msg_prom, NULL);
    return NULL;
}

rpc_state* on_accepted(rpc_state* state) {
    msg_accepted_t *msg_accd;
    msg_accd = mpaxos__msg_accepted__unpack(NULL, state->sz, state->buf);
    log_message_res("receive", "ACCEPTED", msg_accd->h, msg_accd->ress, 
            msg_accd->n_ress, state->sz);
    handle_msg_accepted(msg_accd);
    mpaxos__msg_accepted__free_unpacked(msg_accd, NULL);
    return NULL;
}

rpc_state* on_decide(rpc_state* state) {
    msg_decide_t *msg_dcd = NULL;
    msg_dcd = mpaxos__msg_decide__unpack(NULL, state->sz, state->buf);
    handle_msg_decide(msg_dcd);
    mpaxos__msg_decide__free_unpacked(msg_dcd, NULL);
    return NULL;
}

void start_server(int port) {
    server_create(&server_);
    server_->com.port = port;

    // register function
    server_regfun(server_, RPC_PREPARE, on_prepare);
    server_regfun(server_, RPC_ACCEPT, on_accept);
    server_regfun(server_, RPC_DECIDE, on_decide);
    
    server_start(server_);
    LOG_INFO("Server started on port %d.", port);
    
    connect_all_senders();
}

void set_nid_sender(nodeid_t nid, const char* addr, int port) {
    //Test save the key
    nodeid_t *nid_ptr = apr_pcalloc(mp_global_, sizeof(nid));
    *nid_ptr = nid;
    client_t *c;
    client_create(&c);
    strcpy(c->com.ip, addr);
    c->com.port = port;
    // FIXME register function callbacks. 
    client_regfun(c, RPC_PREPARE, on_promise);
    client_regfun(c, RPC_ACCEPT, on_accepted);
    apr_hash_set(ht_sender_, nid_ptr, sizeof(nid), c);
}
