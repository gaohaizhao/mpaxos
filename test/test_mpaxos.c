/*
 * test_mpaxos.cc
 *
 *  Created on: Jan 29, 2013
 *      Author: ms
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>
#include <json/json.h>
#include <apr_time.h>
#include <pthread.h>
#include <inttypes.h>

#include "mpaxos/mpaxos.h"
#include "mpaxos/mpaxos-config.h"

//This macro should work for gcc version before 4.4
#define __STDC_FORMAT_MACROS
// do not include from ../libmpaxos/utils/logger.h
// it sucks, we should not include from internal library
// if you really want to do that, put the logger into the include/mpaxos directory or this directory as some header lib
// besides, that logger sucks too.
#define LOG_INFO(x, ...) printf("[%s:%d] [ II ] "x"\n", __FILE__, __LINE__, ##__VA_ARGS__)

uint32_t n_tosend = 1000;
int n_group = 1;
int async = 0;
int run = 1;
static int is_exit_ = 0;

void cb(groupid_t gid, slotid_t sid, uint8_t *data, size_t sz_data, void* para) {
    LOG_INFO("asynchronous callback called. gid:%d, sid:%d", gid, sid);
}


void test_group(groupid_t gid) {
    apr_time_t start_time;
    apr_time_t last_time;
    apr_time_t curr_time;

    uint64_t msg_count = 0;
    uint64_t data_count = 0;
    int val_size = 64;
    uint8_t *val = (uint8_t *) calloc(val_size, sizeof(char));

    start_time = apr_time_now();
    last_time = apr_time_now();

    //Keep sending

    int n = n_tosend;
    do {
        if (async) {
            commit_async(&gid, 1, val, val_size, NULL);
        } else {
            commit_sync(&gid, 1, val, val_size);
        }
        msg_count++;
        data_count += val_size;
    } while (--n > 0);
    curr_time = apr_time_now();
    double period = (curr_time - start_time) / 1000000.0;
    double prop_rate = (msg_count + 0.0) / period;
    LOG_INFO("gid:%u, %"PRIu64" proposals commited in %.2fs, rate:%.2f props/s",
            gid, msg_count, period, prop_rate);
    LOG_INFO("%.2f MB data sent, speed:%.2fMB/s",
            (data_count + 0.0) / (1024 * 1024),
            (data_count + 0.0) / (1024 * 1024) /period);
    free(val);
}

int main(int argc, char **argv) {
    // init mpaxos
    mpaxos_init();
    
    int c = 1;
    // initialize from config
    char *config = NULL;
    if (argc > c) {
        config = argv[c];
    } else {
        printf("Usage: %s config [run=1] [n_tosend=1000] [n_group=1] [async=0]\n", argv[0]);
        mpaxos_destroy();
        exit(0);
    }
    int ret = mpaxos_load_config(config);
    if (ret != 0) {
        mpaxos_destroy();
        exit(10);
    }
    
    run = (argc > ++c) ? atoi(argv[c]) : run;
    n_tosend = (argc > ++c) ? atoi(argv[c]) : n_tosend;
    n_group = (argc > ++c) ? atoi(argv[c]) : n_group;
    async = (argc > ++c) ? atoi(argv[c]) : async;
    is_exit_ = (argc > ++c) ? atoi(argv[c]) : is_exit_;
    
    LOG_INFO("test for %d messages.", n_tosend);
    LOG_INFO("test for %d threads.", n_group);
    LOG_INFO("async mode %d", async);

    // start mpaxos service
    mpaxos_set_cb_god(cb);
    mpaxos_start();

    if (run) {
        // wait 2 seconds to wait for everyone starts
        sleep(2);

        LOG_INFO("serve as a master.");
        pthread_t *threads_ptr = malloc(n_group * sizeof(pthread_t));
        apr_time_t begin_time = apr_time_now();

        for (int i = 0; i < n_group; i++) {
            pthread_create(&threads_ptr[i], NULL, 
                    (void* (*)(void*))(test_group),
                    (void*)(intptr_t)(i+1));
        }

        for (int i = 0; i < n_group; i++) {
            pthread_t t = threads_ptr[i];
            pthread_join(t, NULL);
        }
        apr_time_t end_time = apr_time_now();
        double time_period = (end_time - begin_time) / 1000000.0;
        uint64_t n_msg = n_tosend * n_group;
        LOG_INFO("in total %"PRIu64" proposals commited in %.2fsec," 
                "rate:%.2f props per sec.", n_msg, time_period, n_msg / time_period); 

        free(threads_ptr);
    }

    if (exit) {
        apr_sleep(2000000);
        LOG_INFO("Goodbye! I'm about to destroy myself.");
        mpaxos_destroy();
        exit(0);
    }

    LOG_INFO("All my task is done. Go on to serve others.");

    // I want to bring all the evil with me before I go to hell.
    // cannot quit because have to serve for others
    while (true) {
        fflush(stdout);
        apr_sleep(1000000);
    }
}

