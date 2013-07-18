#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>
#include <event.h>
#include <apr-1.0/apr_network_io.h>


#include "utils/safe_assert.h"
#include "utils/logger.h"

//Only needed with cpp
//#include <sys/ioctl.h>

#include "sendrecv.h"

int send_buf_size = 0;
socklen_t optlen;

extern apr_pool_t *pl_global_;
extern apr_pollset_t *pollset_;

void init_sender(sender_t* s) {
    apr_sockaddr_info_get(&s->sa, s->addr, APR_INET, s->port, 0, pl_global_);
/*
    apr_socket_create(&s->s, s->sa->family, SOCK_DGRAM, APR_PROTO_UDP, pl_global_);
*/
    apr_socket_create(&s->s, s->sa->family, SOCK_STREAM, APR_PROTO_TCP, pl_global_);
    apr_socket_opt_set(s->s, APR_SO_NONBLOCK, 1);
    // apr_socket_opt_set(s->s, APR_SO_REUSEADDR, 1);/* this is useful for a server(socket listening) process */
    apr_socket_opt_set(s->s, APR_TCP_NODELAY, 1);
    pthread_mutex_init(&s->mutex, NULL);
    
    s->ctx = get_context();

    printf("Default sending buffer size %d.\n", send_buf_size);
}

void sender_final(sender_t* s) {
    pthread_mutex_destroy(&s->mutex);
}

void connect_sender(sender_t *sender) {
    LOG_DEBUG("connecting to sender %s %d", sender->addr, sender->port);
    apr_status_t status;
    do {
/*
        printf("TCP CLIENT TRYING TO CONNECT.");
*/
        status = apr_socket_connect(sender->s, sender->sa);
    } while (status != APR_SUCCESS);
    LOG_DEBUG("connect socket on remote addr %s, socket %d", sender->addr, sender->s);
    
    // add to epoll
    while (pollset_ == NULL) {
        
    }
    context_t *ctx = sender->ctx;
    ctx->s = sender->s;
    apr_pollfd_t pfd = {pl_global_, APR_POLL_SOCKET, APR_POLLIN, 0, {NULL}, NULL};
    ctx->pfd = pfd;
    ctx->pfd.desc.s = ctx->s;
    ctx->pfd.client_data = ctx;
    status = apr_pollset_add(pollset_, &ctx->pfd);
    SAFE_ASSERT(status == APR_SUCCESS);
}

/*
void do_write(void *arg){
    write_state_t *state = arg;
    sender_t *sender = state->sender;
    
    apr_status_t status;
    size_t n = state->sz_data;
    status = apr_socket_sendto(sender->s, sender->sa, 0, (const char *)state->data, &n);
    SAFE_ASSERT(status == APR_SUCCESS);
    SAFE_ASSERT(n == state->sz_data);
    free(state->data);
    free(state);
    
    // add read to poll
    apr_pollfd_t pfd = {pl_global_, APR_POLL_SOCKET, APR_POLLIN, 0, {NULL}, NULL};
    pfd.desc.s = sender->s;
    apr_pollset_add(pollset_, &pfd);
    ctx_sendrecv_t *ctx = malloc(sizeof(ctx_sendrecv_t));
    apr_pool_create(&ctx->mp, NULL);
    ctx->status = ctx->SERV_RECV_REQUEST;
    ctx->cb_func = do_read;
    apr_pollfd_t pfd = {ctx->mp, APR_POLL_SOCKET, APR_POLLIN, 0, {NULL}, NULL};
    ctx->pfd_recv = pfd;
    ctx->pfd_recv.desc.s = s->s;
    ctx->pfd_recv.client_data = ctx;
    apr_pollset_add(pollset_, &ctx->pfd_recv);
}
*/

/*
int do_read(ctx_sendrecv_t *ctx, apr_pollset_t *pollset, apr_socket_t *sock) {
    // receiv to buf;
    apr_status_t status;
    apr_sockaddr_t remote_sa;
    ctx->recv.buf_recv = apr_palloc(ctx->mp, BUF_SIZE__);
    status = apr_socket_recvfrom(&remote_sa, sock, 0, (char *)ctx->recv.buf_recv, &ctx->recv.sz_buf);
    // TODO [fix] call actual staff.

    // TODO [fix] free memory of context, and remove epoll.
    apr_pollset_remove(pollset_, &ctx->pfd_recv);
    apr_pool_destroy(ctx->mp);
    free(ctx);
}
*/

void msend(sender_t* sender, const uint8_t* msg, size_t msglen) {
    add_write_buf_to_ctx(sender->ctx, msg, msglen);
/*
    // add to pollset
    write_state_t *state = malloc(sizeof(write_state_t));
    state->buf_write = malloc(msglen);
    state->sender = sender;
    state->s = sender->s;
    memcpy(state->buf_write, msg, msglen);
    state->sz_buf_write = msglen;

    apr_pollfd_t pfd = {pl_global_, APR_POLL_SOCKET, APR_POLLOUT | APR_POLLIN, 0, {NULL}, NULL};
    pfd.desc.s = state->s;
    pfd.client_data = state;
    LOG_DEBUG("sender add epoll, pfd socket %d", sender->s);
    apr_status_t status;
    status = apr_pollset_add(pollset_, &pfd);
    if (status != APR_SUCCESS) {
        printf(apr_strerror(status, malloc(100), 100));
        SAFE_ASSERT(0);
    }
*/
}

void mpaxos_send_recv(sender_t* s, const uint8_t* msg, size_t msglen, 
        uint8_t *buf, size_t buf_sz) {
/*
    SAFE_ASSERT(pthread_mutex_lock(&s->mutex) == 0);

    int qb;
    while ((qb = get_queue_bytes(s)) > 0) {
        printf("Wait to send...%d\n", qb);
    }

    //debug
//  printf("send to %s, port %d.\n", s->addr, s->port);
    socklen_t socklen = sizeof(s->servaddr);
    int ret = sendto(s->fd, msg, msglen, 0, 
            (struct sockaddr *)&(s->servaddr), socklen);
    if (ret < 0) {
        printf("error sending %d\n. error:%s", ret, strerror(errno));
    }
    
    recvfrom(s->fd, buf, buf_sz, 0, (struct sockaddr*)&(s->servaddr),
            &socklen);
    SAFE_ASSERT(pthread_mutex_unlock(&s->mutex) == 0);
*/
}
