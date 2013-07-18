/*
 * test_recver.c
 *
 *  Created on: Jan 7, 2013
 *      Author: ms
 */
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "sendrecv.h.h"

uint64_t data_count = 0;
uint64_t msg_count = 0;
time_t start_time = 0;
time_t last_time = 0;
time_t curr_time = 0;

void on_recv(char* buf, size_t size) {
    curr_time = time(NULL);
    if (start_time == 0) {
        start_time = time(0);
        last_time = start_time;
    }
    if (size > 0) {
        data_count += size;
    }
  msg_count += 1;   
    if (curr_time > last_time) {
        float speed = (double)data_count / (1024 * 1024) / (curr_time - start_time);
        printf("%lld messages received. Speed: %.2f MB/s\n", msg_count, speed);
        last_time = curr_time;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: test_recv port\n");
        exit(0);
    }

    recvr_t recvr;
    recvr.msg = (char*) malloc(BUF_SIZE__);
    recvr.port = atoi(argv[1]);
    recvr.on_recv = on_recv;
    init_recvr(&recvr);


    run_recvr_pt(&recvr);
//  run_recvr(&recvr);
    while (1) {
        sleep(1);
    }

    free(recvr.msg);
    return 0;
}
