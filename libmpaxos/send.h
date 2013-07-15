#ifndef SEND_H
#define SEND_H

#include <netinet/in.h>
#include <pthread.h>
#include <apr_network_io.h>

typedef struct sender {
    int fd;
    apr_sockaddr_t *sa;
    apr_socket_t *s;
    char addr[20];
    int port;
    pthread_mutex_t mutex;
} sender_t;

void init_sender(sender_t*);

void msend(sender_t*, const uint8_t*, size_t);

void mpaxos_send_recv(sender_t* s, const uint8_t* msg, size_t msglen, uint8_t* buf, size_t buf_sz);

#endif
