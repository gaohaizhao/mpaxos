
#include <stdbool.h>
#include <apr_hash.h>
#include <apr_thread_proc.h>
#include "comm.h"
#include "view.h"
#include "utils/safe_assert.h"
#include "log_helper.h"
#include "proposer.h"
#include "acceptor.h"
#include "slot_mgr.h"
#include "sendrecv.h"

apr_hash_t *ht_sender_; //nodeid_t -> sender_t

recvr_t *recvr;
pthread_mutex_t mx_comm_;

//View

//uint64_t recv_data_count = 0;
//uint64_t recv_msg_count = 0;
//time_t recv_start_time = 0;
//time_t recv_last_time = 0;
//time_t recv_curr_time = 0;

void comm_init() {
    ht_sender_ = apr_hash_make(mp_global_);
    SAFE_ASSERT(pthread_mutex_init(&mx_comm_, NULL) == 0);
}

void comm_destroy() {
    SAFE_ASSERT(pthread_mutex_destroy(&mx_comm_) == 0);
    //TODO destroy all the hash table
    
    stop_server();
    LOG_DEBUG("stopped listening on network.");

    apr_hash_t *nid_ht = view_group_table(1);
    SAFE_ASSERT(nid_ht != NULL);
    
    apr_hash_index_t *hi;
    for (hi = apr_hash_first(NULL, nid_ht); hi; 
            hi = apr_hash_next(hi)) {
        uint32_t *k, *v;
        apr_hash_this(hi, (const void **)&k, NULL, (void **)&v);
        sender_t *s_ptr = NULL;
        s_ptr = apr_hash_get(ht_sender_, k, sizeof(nodeid_t));
        sender_destroy(s_ptr);
    }
    
    // TODO [fix] destroy all senders and recvrs
    recvr_destroy(recvr);
    if (recvr) {
	    free(recvr);
    }
        
}

void set_nid_sender(nodeid_t nid, const char* addr, int port) {
    //Test save the key
    nodeid_t *nid_ptr = apr_pcalloc(mp_global_, sizeof(nid));
    *nid_ptr = nid;
    sender_t* s_ptr = (sender_t *)apr_pcalloc(mp_global_,
            sizeof(sender_t));
    strcpy(s_ptr->addr, addr);
    s_ptr->port = port;
    sender_init(s_ptr);
    apr_hash_set(ht_sender_, nid_ptr, sizeof(nid), s_ptr);
}

void send_to(nodeid_t nid, msg_type_t type, const uint8_t *data,
    size_t sz) {
    sender_t *s_ptr;
    s_ptr = apr_hash_get(ht_sender_, &nid, sizeof(nid));
    //int hash_size = apr_hash_count(sender_ht_);

    SAFE_ASSERT(s_ptr != NULL);
    msend(s_ptr, type, data, sz);
}

slotid_t send_to_slot_mgr(groupid_t gid, nodeid_t nid, uint8_t *data,
        size_t sz) {
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
}

void send_to_group(groupid_t gid, msg_type_t type, const uint8_t *buf,
    size_t sz) {
	// TODO [FIX] this is not thread safe because of apache hash table
//  apr_hash_t *nid_ht = apr_hash_get(gid_nid_ht_ht_, &gid, sizeof(gid));
	pthread_mutex_lock(&mx_comm_);
    apr_hash_t *nid_ht = view_group_table(gid);
    SAFE_ASSERT(nid_ht != NULL);
    
    apr_hash_index_t *hi;
    for (hi = apr_hash_first(NULL, nid_ht); hi; 
            hi = apr_hash_next(hi)) {
        uint32_t *k, *v;
        apr_hash_this(hi, (const void **)&k, NULL, (void **)&v);
        send_to(*k, type, buf, sz);
    }
    pthread_mutex_unlock(&mx_comm_);
}

void connect_all_senders() {
    apr_hash_t *nid_ht = view_group_table(1);
    SAFE_ASSERT(nid_ht != NULL);
    
    apr_hash_index_t *hi;
    for (hi = apr_hash_first(NULL, nid_ht); hi; 
            hi = apr_hash_next(hi)) {
        uint32_t *k, *v;
        apr_hash_this(hi, (const void **)&k, NULL, (void **)&v);
        sender_t *s_ptr = NULL;
        s_ptr = apr_hash_get(ht_sender_, k, sizeof(nodeid_t));
        connect_sender(s_ptr);
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

void* APR_THREAD_FUNC on_recv(apr_thread_t *th, void* arg) {
//void* APR_THREAD_FUNC on_recv(char* buf, size_t size, char **res_buf, size_t *res_len) {
    struct read_state *state = arg;

    LOG_TRACE("message received. size: %d", state->sz_data);

    // TODO [fix] use the fisrt 1 byte to define message type.
    size_t size = state->sz_data - sizeof(msg_type_t);
    msg_type_t msg_type = 0;
    uint8_t *data = state->data + sizeof(msg_type_t);
    memcpy(&msg_type, state->data, sizeof(msg_type_t));
    
    
    switch(msg_type) {
    case MSG_PREPARE: {
        Mpaxos__MsgPrepare *msg_prep;
        msg_prep = mpaxos__msg_prepare__unpack(NULL, size, data);
        log_message_rid("receive", "PREPARE", msg_prep->h, msg_prep->rids, 
                msg_prep->n_rids, size);
        handle_msg_prepare(msg_prep, &state->buf_write, &state->sz_buf_write);
        mpaxos__msg_prepare__free_unpacked(msg_prep, NULL);
        //return response here.
        state->reply_msg_type = MSG_PROMISE;
        reply_to(state);
        break;
    }
    case MSG_PROMISE: {
        Mpaxos__MsgPromise *msg_prom;
        msg_prom = mpaxos__msg_promise__unpack(NULL, size, data);
        log_message_res("receive", "PROMISE", msg_prom->h, msg_prom->ress, 
                msg_prom->n_ress, size);
        handle_msg_promise(msg_prom);
        mpaxos__msg_promise__free_unpacked(msg_prom, NULL);
        break;
    }
    case MSG_ACCEPT: {
        Mpaxos__MsgAccept *msg_accp;
        msg_accp = mpaxos__msg_accept__unpack(NULL, size, data);
        log_message_rid("receive", "ACCEPT", msg_accp->h, msg_accp->prop->rids,
                msg_accp->prop->n_rids, size);
        handle_msg_accept(msg_accp, &state->buf_write, &state->sz_buf_write);
        mpaxos__msg_accept__free_unpacked(msg_accp, NULL);
        // return response here.
        state->reply_msg_type = MSG_ACCEPTED;
        reply_to(state);
        break;
    }
    case MSG_ACCEPTED: {
        Mpaxos__MsgAccepted *msg_accd;
        msg_accd = mpaxos__msg_accepted__unpack(NULL, size, data);
        log_message_res("receive", "ACCEPTED", msg_accd->h, msg_accd->ress, 
                msg_accd->n_ress, size);
        handle_msg_accepted(msg_accd);
        mpaxos__msg_accepted__free_unpacked(msg_accd, NULL);
        break;
    }
    case MSG_LEARN:
        break;
    case MSG_LEARNED:
        break;
    case MSG_DECIDE:
        break;
    default:
        SAFE_ASSERT(0);
    };
    
    free(state->data);
    free(state);
    return NULL;
}

void start_server(int port) {
    recvr = (recvr_t*)malloc(sizeof(recvr_t));
    recvr->port = port;
    recvr->on_recv = on_recv;
    recvr_init(recvr);

    run_recvr_pt(recvr);
    
    connect_all_senders();
    LOG_INFO("Server started on port %d.", port);
}

