
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

apr_hash_t *sender_ht_; //nodeid_t -> sender_t

recvr_t *recvr;
pthread_mutex_t comm_mutex_;

//View

uint64_t recv_data_count = 0;
uint64_t recv_msg_count = 0;
time_t recv_start_time = 0;
time_t recv_last_time = 0;
time_t recv_curr_time = 0;

void comm_init() {
    sender_ht_ = apr_hash_make(pl_global_);
    SAFE_ASSERT(pthread_mutex_init(&comm_mutex_, NULL) == 0);
}

void comm_final() {
	SAFE_ASSERT(pthread_mutex_destroy(&comm_mutex_) == 0);
	//TODO destroy all the hash table
    if (recvr) {
	    free(recvr);
    }
}

void set_nid_sender(nodeid_t nid, const char* addr, int port) {
    //Test save the key
    nodeid_t *nid_ptr = apr_pcalloc(pl_global_, sizeof(nid));
    *nid_ptr = nid;
    sender_t* s_ptr = (sender_t *)apr_pcalloc(pl_global_,
            sizeof(sender_t));
    strcpy(s_ptr->addr, addr);
    s_ptr->port = port;
    init_sender(s_ptr);
    apr_hash_set(sender_ht_, nid_ptr, sizeof(nid), s_ptr);
}

void send_to(nodeid_t nid, const uint8_t *data,
    size_t sz) {
    sender_t *s_ptr;
    s_ptr = apr_hash_get(sender_ht_, &nid, sizeof(nid));
    //int hash_size = apr_hash_count(sender_ht_);

    SAFE_ASSERT(s_ptr != NULL);
    msend(s_ptr, data, sz);
}

slotid_t send_to_slot_mgr(groupid_t gid, nodeid_t nid, uint8_t *data,
        size_t sz) {
    sender_t *s_ptr;
    s_ptr = apr_hash_get(sender_ht_, &nid, sizeof(nid));

    sender_t *s = (sender_t *)malloc(sizeof(sender_t));
    strcpy(s->addr, s_ptr->addr);
    s->port = s_ptr->port;
    init_sender(s);

    uint8_t* buf = (uint8_t*) malloc(100);
    mpaxos_send_recv(s, data, sz, buf, 100); 
    slotid_t sid = (slotid_t)*buf;
    free(buf);
    free(s);
    return sid; 
}

void send_to_group(groupid_t gid, const uint8_t *buf,
    size_t sz) {
	// TODO [FIX] this is not thread safe because of apache hash table
//  apr_hash_t *nid_ht = apr_hash_get(gid_nid_ht_ht_, &gid, sizeof(gid));
	pthread_mutex_lock(&comm_mutex_);
    apr_hash_t *nid_ht = view_group_table(gid);
    SAFE_ASSERT(nid_ht != NULL);
    
    apr_hash_index_t *hi;
    for (hi = apr_hash_first(NULL, nid_ht); hi; 
            hi = apr_hash_next(hi)) {
        uint32_t *k, *v;
        apr_hash_this(hi, (const void **)&k, NULL, (void **)&v);
        send_to(*k, buf, sz);
    }
    pthread_mutex_unlock(&comm_mutex_);
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
        s_ptr = apr_hash_get(sender_ht_, k, sizeof(nodeid_t));
        connect_sender(s_ptr);
    }
}

void send_to_groups(groupid_t* gids, uint32_t gids_len,
        const char *buf, size_t sz) {
    // TODO [IMPROVE] Optimize
    uint32_t gid;
    uint32_t i;
    for (i = 0; i < gids_len; i++, gids++) {
        gid = *gids;
        send_to_group(gid, (const uint8_t *)buf, sz);
    }
}
void* APR_THREAD_FUNC on_recv(apr_thread_t *th, void* arg) {
//void* APR_THREAD_FUNC on_recv(char* buf, size_t size, char **res_buf, size_t *res_len) {
    struct read_state *state = arg;
    
    LOG_DEBUG("Message received. Size:", state->sz_data);
    recv_curr_time = time(NULL);
    if (recv_start_time == 0) {
        recv_start_time = time(0);
        recv_last_time = recv_start_time;
    }
    recv_data_count += state->sz_data;
    recv_msg_count += 1;
    if (recv_curr_time > recv_last_time) {
        float speed = (double)recv_data_count / (1024 * 1024) / (recv_curr_time - recv_start_time);
        printf("%"PRIu64" messages received. Speed: %.2f MB/s\n", recv_msg_count, speed);
        recv_last_time = recv_curr_time;
    }

    uint8_t *data = state->data;
    size_t size = state->sz_data;
    Mpaxos__MsgCommon *msg_comm_ptr;
    msg_comm_ptr = mpaxos__msg_common__unpack(NULL, size, data);

    SAFE_ASSERT(msg_comm_ptr != NULL);

    if (msg_comm_ptr->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__PROMISE) {
        Mpaxos__MsgPromise *msg_prom_ptr;
        msg_prom_ptr = mpaxos__msg_promise__unpack(NULL, size, data);
        log_message_res("receive", "PROMISE", msg_prom_ptr->ress, msg_prom_ptr->n_ress);
        handle_msg_promise(msg_prom_ptr);
        mpaxos__msg_promise__free_unpacked(msg_prom_ptr, NULL);
    } else if (msg_comm_ptr->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPTED) {
        Mpaxos__MsgAccepted *msg_accd_ptr;
        msg_accd_ptr = mpaxos__msg_accepted__unpack(NULL, size, data);
        log_message_res("receive", "ACCEPTED", msg_accd_ptr->ress, msg_accd_ptr->n_ress);
        handle_msg_accepted(msg_accd_ptr);
        mpaxos__msg_accepted__free_unpacked(msg_accd_ptr, NULL);
    } else if (msg_comm_ptr->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__PREPARE) {
        Mpaxos__MsgPrepare *msg_prep_ptr;
        msg_prep_ptr = mpaxos__msg_prepare__unpack(NULL, size, data);
        log_message_rid("receive", "PREPARE", msg_prep_ptr->rids, msg_prep_ptr->n_rids);
        handle_msg_prepare(msg_prep_ptr, &state->buf_write, &state->sz_buf_write);
        mpaxos__msg_prepare__free_unpacked(msg_prep_ptr, NULL);
        //return response here.
        reply_to(state);
    } else if (msg_comm_ptr->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPT) {
        Mpaxos__MsgAccept *msg_accp_ptr;
        msg_accp_ptr = mpaxos__msg_accept__unpack(NULL, size, data);
        log_message_rid("receive", "ACCEPT", msg_accp_ptr->prop->rids,
                msg_accp_ptr->prop->n_rids);
        handle_msg_accept(msg_accp_ptr, &state->buf_write, &state->sz_buf_write);
        mpaxos__msg_accept__free_unpacked(msg_accp_ptr, NULL);
        // return response here.
        reply_to(state);
    } else if (msg_comm_ptr->h->t == MPAXOS__MSG_HEADER__MSGTYPE_T__SLOT) {
        Mpaxos__MsgSlot *msg_slot_ptr;
        msg_slot_ptr = mpaxos__msg_slot__unpack(NULL, size, data);

        groupid_t gid = msg_slot_ptr->h->pid->gid;
        nodeid_t nid = msg_slot_ptr->h->pid->nid;
        LOG_DEBUG("receive SLOT message from nid %u of group gid %u.", nid, gid);

//        if (is_slot_mgr(gid)) {
//            slotid_t sid = alloc_slot(gid, nid);    
//            char *buf = malloc(sizeof(slotid_t));
//            memcpy(buf, &sid, sizeof(slotid_t));
//            *res_len = sizeof(slotid_t);
//            *res_buf = buf; 
//        } else {
//                
//        }
        mpaxos__msg_slot__free_unpacked(msg_slot_ptr, NULL);
    } else {
        LOG_DEBUG("Unknown message received. Fuck!");
        SAFE_ASSERT(0);
    }
    mpaxos__msg_common__free_unpacked(msg_comm_ptr, NULL);
    
    free(state->data);
    free(state);
    return NULL;
}

void start_server(int port) {
    recvr = (recvr_t*)malloc(sizeof(recvr_t));
    recvr->port = port;
    recvr->on_recv = on_recv;
    init_recvr(recvr);

    run_recvr_pt(recvr);
    
    connect_all_senders();
    LOG_INFO("Server started on port %d.", port);
}

