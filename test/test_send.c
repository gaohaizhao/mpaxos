/*
 * test_sender.cc
 *
 *  Created on: Jan 7, 2013
 *      Author: ms
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "send.h"


int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage:test_send host port size.\n");
        exit(0);
    }
    sender_t sender;
    strcpy(sender.addr, argv[1]);
    int port = atoi(argv[2]);
    sender.port = port;
    init_sender(&sender);
    
    int size;
    if (argc >= 4) {
        size = atoi(argv[3]);
    } else {
        size = 8192;
    }

    printf("Prepare to send to %s, port %d, size %d.\n", sender.addr, sender.port, size);
    char* msg = (char*)malloc(size);
    strcpy(msg, "Hello world.\n");
    int i;
    uint64_t count = 0;
    for (i = 1; 1; i++) {
        msend(&sender, msg, size);
        count ++;
        if (count % 10000 == 0) {
            printf("%lld messages sent.\n", count);
        }
    //  printf("Message %d sent.\n", i);
    }
    free(msg);
}
