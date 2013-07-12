/**
 * compile this with:
 * gcc test_pthreadpool.c `pkg-config --cflags --libs apr-1 apr-util-1` -l pthread --std=c99 &> tmp
 */


#include <pthread.h>
#include <apr_thread_proc.h>
#include <apr_thread_pool.h>
#include <apr_time.h>

#define THREAD_NUM 1000

pthread_cond_t cond;
pthread_mutex_t mutex;
apr_pool_t *pl_;
apr_thread_pool_t *tp_;



void do_job() {
//    sleep(1);
}

void* APR_THREAD_FUNC do_job_apr(apr_thread_t *th, void *v) {
    do_job();
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}

void* APR_THREAD_FUNC do_job_apr_tp(apr_thread_t *th, void *v) {
    do_job();
    return NULL;
}

void do_job_pthread() {
    do_job();
}


int main() {
    printf("TESTING 100 PTHREADS.\n");
    apr_time_t t1 = apr_time_now();
    pthread_t threads[100];
    for (int i = 0; i < 100; i++) {
        pthread_create(&threads[i], NULL, (void *)do_job_pthread, NULL);
    }
    
    for (int i = 0; i < 100; i++) {
        pthread_join(threads[i], NULL);
    }
    apr_time_t t2 = apr_time_now();
    double period = (t2 - t1);
    printf("100 PTHREADS COST %.3f mircos\n", period);
    
    apr_initialize();
    apr_pool_create(&pl_, NULL);
    
    printf("TESTING 100 APR_THREADS\n");
    t1 = apr_time_now();
    apr_thread_t *aprthreads[100];
    for (int i = 0; i < 100; i++) {
        apr_thread_create(&aprthreads[i], NULL, do_job_apr, NULL, pl_);
    }
    for (int i = 0; i < 100; i++) {
        apr_status_t s;
        apr_thread_join(&s, aprthreads[i]);
    }
    t2 = apr_time_now();
    period = (t2 - t1);
    printf("100 APR_THREADS COST %.3f micros\n", period);
    
    
    apr_thread_pool_create(&tp_, 100, 100, pl_);
    printf("TESTING 100 APR_THREADS IN THREAD POOL.\n");
    t1 = apr_time_now();
    for (int i = 0; i <100; i++) {
        apr_thread_pool_push(tp_, do_job_apr_tp, NULL, 0, NULL);
    }
    while (apr_thread_pool_tasks_count(tp_) > 0) {
        sleep(0);
    }
    apr_thread_pool_destroy(tp_);
    t2 = apr_time_now();
    period = (t2 - t1);
    printf("100 APR_THREADS IN APR_THREADS POOL COST %.3f micros\n", period);
}