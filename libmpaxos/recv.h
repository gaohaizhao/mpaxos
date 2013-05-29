#ifndef RECVR_H
#define RECVR_H

#include <netinet/in.h>


#define BUF_SIZE__ (10 * 1024 * 1024)

typedef struct {
    int fd;
    struct sockaddr_in servaddr;
    int port;
    void (*on_recv)(char*, size_t, char**, size_t*);
    char *msg;
} recvr_t;

void init_recvr(recvr_t* r);

void* run_recvr(recvr_t* r);

void run_recvr_pt(recvr_t* r);

void stop_server();

#endif
