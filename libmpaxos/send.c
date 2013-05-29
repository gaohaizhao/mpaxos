#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <errno.h>

#include "utils/safe_assert.h"

//Only needed with cpp
//#include <sys/ioctl.h>

#include "send.h"

int send_buf_size = 0;
socklen_t optlen;


void init_sender(sender_t* s) {
    memset(&s->servaddr, 0, sizeof(struct sockaddr_in));
    s->servaddr.sin_family = AF_INET;
    s->servaddr.sin_port = htons(s->port);
    s->servaddr.sin_addr.s_addr = inet_addr(s->addr);
    s->fd = socket(AF_INET, SOCK_DGRAM, 0);

    //get then sending buffer size.
    optlen = sizeof(send_buf_size);
    getsockopt(s->fd, SOL_SOCKET, SO_SNDBUF, &send_buf_size, &optlen);

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

void msend(sender_t* s, const uint8_t* msg, size_t msglen) {
    SAFE_ASSERT(pthread_mutex_lock(&s->mutex) == 0);

    int qb;
    while ((qb = get_queue_bytes(s)) > 0) {
        printf("Wait to send...%d\n", qb);
    }

    //debug
//  printf("send to %s, port %d.\n", s->addr, s->port);
    socklen_t socklen = sizeof(s->servaddr);
    int ret = sendto(s->fd, msg, msglen, 0, (struct sockaddr *)&(s->servaddr), socklen);
    if (ret < 0) {
        printf("error sending %d\n. error:%s", ret, strerror(errno));
    }
    SAFE_ASSERT(pthread_mutex_unlock(&s->mutex) == 0);
}

void mpaxos_send_recv(sender_t* s, const uint8_t* msg, size_t msglen, 
        uint8_t *buf, size_t buf_sz) {
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
}
