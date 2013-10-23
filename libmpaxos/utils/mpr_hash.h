/* 
 * File:   mpr_hash.h
 * Author: ms
 * 
 * A thread-safe hash table that manage its key and value for itself.
 *
 * Created on October 2, 2013, 6:28 PM
 */

#ifndef MPR_HASH_H
#define	MPR_HASH_H

#include <apr_hash.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
    apr_pool_t *mp;
    apr_thread_mutex_t *mx;
    apr_hash_t *ht;
} mpr_hash_t;    

typedef struct {
    void *key;
    size_t sz_key;
    void *value;
    size_t sz_value;
} mpr_hash_value_t;

static void mpr_hash_create(mpr_hash_t **hash) {
    *hash = calloc(sizeof(mpr_hash_t), 1);
    apr_pool_create(&(*hash)->mp, NULL);
    (*hash)->ht = apr_hash_make((*hash)->mp);
    apr_thread_mutex_create(&(*hash)->mx, APR_THREAD_MUTEX_UNNESTED, (*hash)->mp);
}

static void mpr_hash_destroy(mpr_hash_t *hash) {
    // TODO [fix] remove all keys.
    
    apr_hash_index_t *hi;
    for (hi = apr_hash_first(NULL, hash->ht); hi; 
            hi = apr_hash_next(hi)) {
        void *k = NULL;
        mpr_hash_value_t *v = NULL;
        apr_hash_this(hi, (const void **)&k, NULL, (void **)&v);
        free(v->key);
        free(v->value);
        free(v);
    }
    
    apr_thread_mutex_destroy(hash->mx);
    apr_pool_destroy(hash->mp);
    free(hash);
}

static void mpr_hash_set(mpr_hash_t *hash, const void *key, size_t sz_key, 
        const void *value, size_t sz_value) {
    apr_thread_mutex_lock(hash->mx);
    
    // delete the old value.
    mpr_hash_value_t *v_old = apr_hash_get(hash->ht, key, sz_key);
    if (v_old != NULL) {
        apr_hash_set(hash->ht, v_old->key, v_old->sz_key, NULL);
        free(v_old->key);
        free(v_old->value);
        free(v_old);
    }
    
    
    mpr_hash_value_t *v_new = NULL;
    if (value != NULL) {
        LOG_DEBUG("set new value in hast table.");
        v_new = malloc(sizeof(mpr_hash_value_t));
        v_new->key = malloc(sz_key);
        v_new->value = malloc(sz_value);
        memcpy(v_new->key, key, sz_key);
        memcpy(v_new->value, value, sz_value);
        v_new->sz_key = sz_key;
        v_new->sz_value = sz_value;
        apr_hash_set(hash->ht, v_new->key, v_new->sz_key, v_new);
    } else {
        // delete the value.
    }
    
    apr_thread_mutex_unlock(hash->mx);
}

static void mpr_hash_get(mpr_hash_t *hash, const void *key, size_t sz_key, 
        void **value, size_t *sz_value) {
    apr_thread_mutex_lock(hash->mx);
    
    mpr_hash_value_t *v_old = apr_hash_get(hash->ht, key, sz_key);
    if (v_old != NULL) {
        *value = v_old->value;
        *sz_value = v_old->sz_value;
    } else {
        *value = NULL;
        *sz_value = 0;
    }
    
    apr_thread_mutex_unlock(hash->mx);    
}


#ifdef	__cplusplus
}
#endif

#endif	/* MPR_HASH_H */

