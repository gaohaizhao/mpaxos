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
#include <assert.h>
#include <inttypes.h>
#include <apr_atomic.h>
#include <apr_thread_proc.h>
#include <apr_thread_pool.h>

#include "mpaxos/mpaxos.h"
#include "mpaxos/mpaxos-config.h"

#define MAX_THREADS 100

//This macro should work for gcc version before 4.4
#define __STDC_FORMAT_MACROS
// do not include from ../libmpaxos/utils/logger.h
// it sucks, we should not include from internal library
// if you really want to do that, put the logger into the include/mpaxos directory or this directory as some header lib
// besides, that logger sucks too.
#define LOG_INFO(x, ...) printf("[%s:%d] [ II ] "x"\n", __FILE__, __LINE__, ##__VA_ARGS__)

uint32_t n_tosend = 1000;
static int n_group = 0;
static uint32_t group_begin_ = 1;
int async = 0;
int run = 1;
static int is_exit_ = 0;
static int t_sleep_ = 2;

static apr_pool_t *pl_test_;
static apr_thread_pool_t *tp_test_;

static uint8_t TEST_DATA[100];
static size_t SZ_DATA = 100;
static int ready_to_exit = 0;

static apr_time_t time_begin_;
static apr_time_t time_end_;

static apr_uint32_t n_group_running;
static apr_uint32_t n_batch_ = 1;

void destroy();

void exit_on_finish() {
    if (is_exit_) {
        apr_sleep(2000000);
        LOG_INFO("Goodbye! I'm about to destroy myself.");
        if (!async) {
            destroy();
        }
        mpaxos_destroy();
        LOG_INFO("Lalala");
        exit(0);
    } else {
        LOG_INFO("All my task is done. Go on to serve others.");

        // I want to bring all the evil with me before I go to hell.
        // cannot quit because have to serve for others
        while (true) {
            fflush(stdout);
            apr_sleep(1000000);
        }
    }
}

void stat_result() {
    double period = (time_end_ - time_begin_) / 1000000.0;
    int period_ms = (time_end_ - time_begin_) / 1000;
    uint64_t msg_count = n_tosend * n_group;
    uint64_t data_count = msg_count * SZ_DATA;
    double prop_rate = (msg_count + 0.0) / period;
    LOG_INFO("%"PRIu64" proposals commited in %dms, rate:%.2f props/s",
            msg_count, period_ms, prop_rate);
    LOG_INFO("%.2f MB data sent, speed:%.2fMB/s",
            (data_count + 0.0) / (1024 * 1024),
            (data_count + 0.0) / (1024 * 1024) /period);
}

void cb(groupid_t* gids, size_t sz_gids, slotid_t* sids, 
        uint8_t *data, size_t sz_data, void* para) {
    uint32_t n_left = (uint32_t)(uintptr_t)para;
    if (n_left-- > 0) {
        commit_async(gids, sz_gids, TEST_DATA, SZ_DATA, (void*)(uintptr_t)n_left);
    } else {
        apr_atomic_dec32(&n_group_running);
        if (apr_atomic_read32(&n_group_running) == 0) {
            time_end_ = apr_time_now();
            stat_result();
            ready_to_exit = 1;
        }
    }
}

void test_async_start() {
    apr_atomic_set32(&n_group_running, n_group);
    time_begin_ = apr_time_now();
    for (int i = 1; i <= n_group; i++) {
        groupid_t *gids = (groupid_t*) malloc(n_batch_ * sizeof(groupid_t));
        groupid_t gid_start = (i * n_batch_) + group_begin_;
        for (int j = 0; j < n_batch_; j++) {
            gids[j] = gid_start + j;
        }
        commit_async(gids, n_batch_, TEST_DATA, SZ_DATA, (void*)(uintptr_t)(n_tosend-1));
    }
    
    while (1) {
        while (!ready_to_exit) {
            apr_sleep(2000000);
        }
        exit_on_finish();
    }
}

void* APR_THREAD_FUNC test_group(apr_thread_t *p_t, void *v) {
    groupid_t gid = (groupid_t)(uintptr_t)v;
    apr_time_t start_time;
    apr_time_t curr_time;

    uint64_t msg_count = 0;
    uint64_t data_count = 0;
    int val_size = 64;
    uint8_t *val = (uint8_t *) calloc(val_size, sizeof(char));

    start_time = apr_time_now();

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
    LOG_INFO("gid:%"PRIu64", %"PRIu64" proposals commited in %.2fs, rate:%.2f props/s",
            gid, msg_count, period, prop_rate);
    LOG_INFO("%.2f MB data sent, speed:%.2fMB/s",
            (data_count + 0.0) / (1024 * 1024),
            (data_count + 0.0) / (1024 * 1024) /period);
    free(val);
    return NULL;
}

void init_thread_pool() {
    apr_initialize();
    apr_pool_create(&pl_test_, NULL);
    apr_thread_pool_create(&tp_test_, MAX_THREADS, MAX_THREADS, pl_test_);
    
}

void destroy() {
    apr_pool_destroy(pl_test_);
}


int main(int argc, char **argv) {
    
    int c = 1;
    // initialize from config
    char *config = NULL;
    if (argc > c) {
        config = argv[c];
    } else {
        printf("Usage: %s config [run=1] [n_tosend=1000] [n_group=1] "
                "[async=0] [is_exit_=0] [sleep_time=2] [group_begin=1] "
                "[n_batch=1]\n", argv[0]);
        exit(0);
    }
    
    // init mpaxos
    mpaxos_init();
    
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
    t_sleep_ = (argc > ++c) ? atoi(argv[c]) : t_sleep_;
    group_begin_ = (argc > ++c) ? atoi(argv[c]) : group_begin_;
    n_batch_ = (argc > ++c) ? atoi(argv[c]) : n_batch_;
    
    LOG_INFO("test for %d messages.", n_tosend);
    LOG_INFO("test for %d threads.", n_group);
    LOG_INFO("async mode %d", async);

    // start mpaxos service
    mpaxos_set_cb_god(cb);
    mpaxos_start();

    if (run) {
        // wait some time to wait for everyone starts
        sleep(t_sleep_);
        
        LOG_INFO("i have something to propose.");
        
        if (async) {
            test_async_start();
        } else {
            init_thread_pool();

            apr_time_t begin_time = apr_time_now();

            for (uint32_t i = 0; i < n_group; i++) {
                apr_thread_pool_push(tp_test_, test_group, (void*)(uintptr_t)(i+1), 0, NULL);
            }
            while (apr_thread_pool_tasks_count(tp_test_) > 0) {
                sleep(0);
            }

            apr_thread_pool_destroy(tp_test_);
            apr_time_t end_time = apr_time_now();
            double time_period = (end_time - begin_time) / 1000000.0;
            uint64_t n_msg = n_tosend * n_group;
            LOG_INFO("in total %"PRIu64" proposals commited in %.2fsec," 
                    "rate:%.2f props per sec.", n_msg, time_period, n_msg / time_period); 
        }
    }

    exit_on_finish();
}

