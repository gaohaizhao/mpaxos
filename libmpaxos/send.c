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

#include "send.h"

int send_buf_size = 0;
socklen_t optlen;

extern apr_pool_t *pl_global_;


void init_sender(sender_t* s) {
    apr_sockaddr_info_get(&s->sa, s->addr, APR_INET, s->port, 0, pl_global_);
    apr_socket_create(&s->s, s->sa->family, SOCK_DGRAM, APR_PROTO_UDP, pl_global_);
/*
    apr_socket_create(&s->s, s->sa->family, SOCK_SEQPACKET, APR_PROTO_SCTP, pl_global_);
*/
    apr_socket_opt_set(s->s, APR_SO_NONBLOCK, 0);
    pthread_mutex_init(&s->mutex, NULL);

    printf("Default sending buffer size %d.\n", send_buf_size);
}

void sender_final(sender_t* s) {
    pthread_mutex_destroy(&s->mutex);
}

int get_queue_bytes(sender_t* s) {
    int value = 0;
    ioctl(s->fd, TIOCOUTQ, &value);
    //ioctl(s->fd, SIOCOUTQ, &value);
#ifdef __MACH__
#else
#endif
    return value;
}

struct write_state {
    uint8_t *data;
    size_t  sz_data;
    sender_t *sender;
};

void do_write(void *arg){
    struct write_state *state = arg;
    sender_t *s = state->sender;
    apr_socket_sendto(s->s, s->sa, 0, (const char *)state->data, &state->sz_data);
    free(state->data);
    free(state);
}

void msend(sender_t* s, const uint8_t* msg, size_t msglen) {
    SAFE_ASSERT(pthread_mutex_lock(&s->mutex) == 0);
    // TODO send to target using libevent
    struct write_state *state = malloc(sizeof(struct write_state));
    state->sender = s;
    state->data = malloc(msglen);
    memcpy(state->data, msg, msglen);
    state->sz_data = msglen;
    do_write(state);
    SAFE_ASSERT(pthread_mutex_unlock(&s->mutex) == 0);
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
