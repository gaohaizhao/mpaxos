
#include <apr_queue.h>
#include <apr_thread_mutex.h>


typedef struct {
    apr_pool_t *mp; 
    apr_queue_t *qu;
    apr_thread_mutex_t *mx;
    apr_thread_t *th;
    void *func;
    apr_uint32_t is_exit;
} mpr_thread_pool_t;

mpr_thread_pool_create(mpr_thread_pool_t **tp, void *func) {
    *tp = malloc(sizeof(mpr_thread_pool_t));
    apr_pool_create(&tp->mp);
    apr_queue_create(&(*tp)->qu, 1000000, (*tp)->mp);
    apr_thread_mutex_create(&(*tp)->mx, APR_THREAD_MUTEX_UNNESTED, (*tp)->mp);
    apr_atomic_set32(&tp->is_exit, 0);
    apr_thread_create(&(*tp)->th, NULL, mpr_thread_pool_run, NULL);  
}

mpr_thread_pool_destroy(mpr_thread_pool_t *tp) {
    apr_atomic_set32(&tp->is_exit, 1);
    apr_queue_term(tp->qu);
    apr_thread_join(tp->th);  
    apr_thread_mutex_destroy(tp->mx);        
    apr_pool_destroy(tp->mp);
    free(tp);
}

mpr_thread_pool_run() {
    mpr_thread_pool_t *tp = NULL;
    while (apr_atomic_read32(tp->is_exit) == 0) {
        void *param;
        apr_status_t status = APR_SUCCESS;
        status = apr_queue_pop(tp->qu, (void**)&param);
        if (status != APR_SUCCESS) {
            LOG_ERROR("queue pop not successful: %s", apr_strerror(status, calloc(100), 100));
            SAFE_ASSERT(0);
        }
        tp->func(param)  
    }
}

mpr_thread_pool_push(mpr_thread_pool_t *tp, void *params) {
    apr_queue_push(tp->qu, params)   
}
