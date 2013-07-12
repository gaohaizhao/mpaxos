#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <event.h>
#include <event2/thread.h>
#include <apr_thread_proc.h>
#include <errno.h>
#include "recv.h"
#include "utils/logger.h"

#define MAX_THREADS 100

extern apr_pool_t *pl_global_;
apr_thread_t *t;
struct event_base *ev_base_;
struct event ev_listener_;

void init_recvr(recvr_t* r) {
    memset(&r->servaddr, 0, sizeof(struct sockaddr_in));
    r->servaddr.sin_family = AF_INET;
    r->servaddr.sin_addr.s_addr = INADDR_ANY;
    r->servaddr.sin_port = htons(r->port);
    apr_pool_create(&r->pl_recv, NULL);
    apr_thread_pool_create(&r->tp_recv, MAX_THREADS, MAX_THREADS, r->pl_recv);

    r->fd = socket(AF_INET, SOCK_DGRAM, 0);
    evutil_make_socket_nonblocking(r->fd);
    
    if (bind(r->fd, (struct sockaddr *)&r->servaddr, 
            sizeof(struct sockaddr_in)) < 0) {
        LOG_ERROR("cannot bind.");
        exit(0);
    }

    int recv_buf_size = BUF_SIZE__;
    socklen_t optlen = sizeof(recv_buf_size);
    setsockopt(r->fd, SOL_SOCKET, SO_SNDBUF, &recv_buf_size, optlen);

    r->msg = (char*)malloc(BUF_SIZE__);
}

void on_accept(int fd, short event, void *arg) {
    recvr_t *r = (recvr_t*)arg;
    int n;
    struct sockaddr_in cliaddr;
    

    socklen_t len = sizeof(cliaddr);
//    LOG_DEBUG("start reading socket");
    n = recvfrom(r->fd, r->msg, BUF_SIZE__, 0, (struct sockaddr *)&cliaddr, &len);
//    LOG_DEBUG("finish reading socket.");
    if (n < 0) {
        if (errno == EAGAIN) {
            return;
        }
        LOG_WARN("error when receiving message:%s", strerror(errno));
    } else if (n == 0) {
        LOG_WARN("received an empty message.");
    } else {
        struct read_state *state = malloc(sizeof(struct read_state));
        state->sz_data = n;
        state->data = malloc(n);
        memcpy(state->data, r->msg, n);
        state->recvr = r;
        
//        apr_thread_t *t;
//        apr_thread_create(&t, NULL, r->on_recv, (void*)state, r->pl_recv);
        apr_thread_pool_push(r->tp_recv, r->on_recv, (void*)state, 0, NULL);
//        (*(r->on_recv))(NULL, state);
        
//        size_t res_sz = 0;      
//        char *res_buf = NULL;
//        
//        (*(r->on_recv))(r->msg, n, &res_buf, &res_sz);
//        if (res_sz > 0) {
//            sendto(r->fd, res_buf, res_sz, 0, (struct sockaddr *)&cliaddr, len);
//            free(res_buf);
//        }
    }
//  printf("Event id :%d.\n", event);
}

void* APR_THREAD_FUNC run_recvr(apr_thread_t *t, void* v) {
    recvr_t* r = v;
//    evthread_use_pthreads();
    ev_base_ = event_base_new();
    event_set(&ev_listener_, r->fd, EV_READ|EV_PERSIST, on_accept, (void*)r);
    event_base_set(ev_base_, &ev_listener_);
    event_add(&ev_listener_, NULL);
    event_base_dispatch(ev_base_);
    LOG_DEBUG("event loop ends.");
    return NULL;
}

void stop_server() {
    if (t) {
        LOG_DEBUG("try to stop server, but do not know how.");
        event_del(&ev_listener_);
//        event_base_loopexit(ev_base_, NULL);
        event_base_loopbreak(ev_base_);
//        LOG_DEBUG("event base got exit? %d", event_base_got_exit(ev_base_));
        LOG_DEBUG("event base got break? %d", event_base_got_break(ev_base_));
        
//        apr_thread_join(NULL, t);
        LOG_DEBUG("recv server ends.");
    }
}

void run_recvr_pt(recvr_t* r) {
    apr_thread_create(&t, NULL, run_recvr, (void*)r, pl_global_);
}
