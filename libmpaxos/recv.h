#ifndef RECVR_H
#define RECVR_H

#include <netinet/in.h>
#include <apr_thread_proc.h>
#include <apr_thread_pool.h>

#define BUF_SIZE__ (10 * 1024 * 1024)

typedef struct {
    int fd;
    struct sockaddr_in servaddr;
    int port;
    void* APR_THREAD_FUNC (*on_recv)(apr_thread_t *th, void* arg);
    char *msg;
    apr_pool_t *pl_recv;
    apr_thread_pool_t *tp_recv;

} recvr_t;

struct read_state {
//    struct event *ev_read;
    uint8_t *data;
    size_t  sz_data;
    recvr_t *recvr;
};

void init_recvr(recvr_t* r);

void* APR_THREAD_FUNC run_recvr(apr_thread_t *t, void* v);

void run_recvr_pt(recvr_t* r);

void stop_server();

#endif
