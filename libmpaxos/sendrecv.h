#ifndef SENDRECV_H
#define SENDRECV_H

#include <netinet/in.h>
#include <apr_thread_proc.h>
#include <apr_thread_pool.h>
#include <apr_poll.h>

#define BUF_SIZE__ (10 * 1024 * 1024)


typedef struct {
    apr_socket_t *s;
    void* APR_THREAD_FUNC (*on_recv)(apr_thread_t *th, void* arg);

    apr_pool_t *mp;
    apr_thread_mutex_t *mx;
    
    struct  {
        uint8_t *buf;
        size_t sz;
        size_t offset_begin;
        size_t offset_end;
    } buf_send;
    
    struct  {
        uint8_t *buf;
        size_t sz;
        size_t offset_begin;
        size_t offset_end;
    } buf_recv;
    
    apr_pollfd_t pfd;
} context_t;

typedef struct {
    apr_sockaddr_t *sa;
    apr_socket_t *s;
    
    char addr[20];
    int port;
    
    void* APR_THREAD_FUNC (*on_recv)(apr_thread_t *th, void* arg);
    pthread_mutex_t mutex;
    
    apr_pool_t *pl_recv;
    
//    struct  {
//        uint8_t *buf;
//        size_t sz;
//        size_t offset_begin;
//        size_t offset_end;
//    } buf_send;
//    
//    struct  {
//        uint8_t *buf;
//        size_t sz;
//        size_t offset_begin;
//        size_t offset_end;
//    } buf_recv;

    context_t **ctxs;
    size_t sz_ctxs;
} recvr_t;


typedef struct {
    apr_sockaddr_t *sa;
    apr_socket_t *s;
    
    char addr[20];
    int port;
    
    void* APR_THREAD_FUNC (*on_recv)(apr_thread_t *th, void* arg);
    
    apr_pool_t *mp;
    
    context_t *ctx;
    
} sender_t;

typedef struct read_state {
//    struct event *ev_read;
    uint8_t *data;
    size_t  sz_data;
    uint8_t *buf_write;
    size_t sz_buf_write;
    context_t *ctx;
} read_state_t;

typedef struct {
    uint8_t *data;
    size_t  sz_data;
    uint8_t *buf_write;
    size_t sz_buf_write;
    sender_t *sender;
    apr_sockaddr_t remote_sa;
    apr_socket_t *s;
} write_state_t;



//typedef struct ctx_sendrecv {
//    enum {
//        SERV_RECV_REQUEST,
//        SERV_SEND_RESPONSE,
//    } status;
//
//    socket_callback_t cb_func;
//    apr_pollfd_t pfd_send;
//    apr_pollfd_t pfd_recv;
//    apr_pool_t *mp;
//
//    /* recv ctx */
//    struct {
//        uint8_t *buf_recv;
//        size_t sz_buf;
//        int is_firstline;
//    } recv;
//
//    /* send ctx */
//    struct {
//        uint8_t *send_buf;
//        size_t sz_buf;
//        apr_off_t offset;
//    } send;
//} ctx_sendrecv_t;
//
//typedef int (*socket_callback_t)(ctx_sendrecv_t *ctx, apr_pollset_t *pollset, apr_socket_t *sock);


void recvr_init(recvr_t* r);

void recvr_destroy(recvr_t* r);

void* APR_THREAD_FUNC run_recvr(apr_thread_t *t, void* v);

void run_recvr_pt(recvr_t* r);

void stop_server();

void reply_to(read_state_t *state);

void sender_init(sender_t*);

void sender_destroy(sender_t *);

void msend(sender_t*, const uint8_t*, size_t);

void mpaxos_send_recv(sender_t* s, const uint8_t* msg, size_t msglen, uint8_t* buf, size_t buf_sz);

void connect_sender(sender_t *sender);

void add_write_buf_to_ctx(context_t *ctx, const uint8_t *buf, size_t sz_buf);

context_t *context_gen();

void context_destroy(context_t *ctx);

#endif


