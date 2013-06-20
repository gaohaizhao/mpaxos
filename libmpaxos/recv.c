#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <event.h>
#include <event2/thread.h>
#include <pthread.h>
#include <errno.h>
#include "recv.h"
#include "utils/logger.h"

pthread_t t;
struct event_base *ev_base_;
struct event ev_listener_;

void init_recvr(recvr_t* r) {
    memset(&r->servaddr, 0, sizeof(struct sockaddr_in));
    r->servaddr.sin_family = AF_INET;
    r->servaddr.sin_addr.s_addr = INADDR_ANY;
    r->servaddr.sin_port = htons(r->port);
    
    r->fd = socket(AF_INET, SOCK_DGRAM, 0);
    bind(r->fd, (struct sockaddr *)&r->servaddr, sizeof(struct sockaddr_in));

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
    if (n != -1) {
        size_t res_sz = 0;      
        char *res_buf = NULL;
        (*(r->on_recv))(r->msg, n, &res_buf, &res_sz);
        if (res_sz > 0) {
            sendto(r->fd, res_buf, res_sz, 0, (struct sockaddr *)&cliaddr, len);
            free(res_buf);
        }
    } else {
        LOG_WARN("received msg size -1, error:%s", strerror(errno));
    }
//  printf("Event id :%d.\n", event);
}

void *run_recvr(recvr_t* r) {
    evthread_use_pthreads();
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
        event_base_loopexit(ev_base_, NULL);
        event_base_loopbreak(ev_base_);
        
        LOG_DEBUG("event base got exit? %d", event_base_got_exit(ev_base_));
        LOG_DEBUG("event base got break? %d", event_base_got_break(ev_base_));
        
        pthread_join(t, NULL);
        LOG_DEBUG("recv server ends.");
    }
}

void run_recvr_pt(recvr_t* r) {
    pthread_create(&t, NULL, (void* (*)(void*))(run_recvr), (void *)r);
}
