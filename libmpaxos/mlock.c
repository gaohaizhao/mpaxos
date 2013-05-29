
#include <pthread.h>
#include <apr_hash.h>
#include "utils/mlock.h"

static pthread_mutex_t mx_index_ = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mx_init_ = PTHREAD_MUTEX_INITIALIZER;
static int is_init_ = 0;

static apr_pool_t *pl_index_;
static apr_hash_t *ht_index_;

void mlock_init() {
    pthread_mutex_lock(&mx_init_);
    if (!is_init_) {
        apr_initialize(); // This is to make sure.
        apr_pool_create(&pl_index_, NULL);
        ht_index_ = apr_hash_make(pl_index_);
        is_init_ = 1;
    }
    pthread_mutex_unlock(&mx_init_);

}

pthread_mutex_t* m_getlock(char* name) {
    mlock_init();
    pthread_mutex_lock(&mx_index_);
    pthread_mutex_t *v = apr_hash_get(ht_index_, name, strlen(name));
    if (v == NULL) {
        char *k = apr_pcalloc(pl_index_, strlen(name) + 1);
        strcpy(k, name);
        v = apr_pcalloc(pl_index_, sizeof(pthread_mutex_t));
        pthread_mutex_init(v, NULL);
        apr_hash_set(ht_index_, k, strlen(k), v);
    }
    pthread_mutex_unlock(&mx_index_);
    return v;
}

void m_lock(char *name) {
    pthread_mutex_t* lock = m_getlock(name);
    pthread_mutex_lock(lock);
}


void m_unlock(char *name) {
    pthread_mutex_t* lock = m_getlock(name);
    pthread_mutex_unlock(lock);
}
