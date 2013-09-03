/* 
 * File:   queue.h
 * Author: ms
 *
 * APR queue is good, but it has two flaws: 1) does not support peek/glance; 
 * 2) has a maximum capacity limitation. This is targeted to solve the two issues.
 * 
 * Created on August 20, 2013, 5:36 PM
 */

#ifndef QUEUE_H
#define	QUEUE_H

#include <apr_thread_mutex.h>

typedef struct {
    apr_queue_t *queue;
    apr_thread_mutex_t *mx;
    void *head;
} mpr_queue_t;

static void mpr_queue_create(mpr_queue_t **queue,
        int32_t capacity, apr_pool_t *pl) {
	*queue = (mpr_queue_t *) malloc(sizeof(mpr_queue_t));
    apr_queue_create(&(*queue)->queue, capacity, pl);
    apr_thread_mutex_create(&(*queue)->mx, APR_THREAD_MUTEX_UNNESTED, pl);
    (*queue)->head = NULL;
}

static apr_status_t mpr_queue_trypop(mpr_queue_t *queue, void** data) {
    apr_thread_mutex_lock(queue->mx);
    apr_status_t status;
    if (queue->head != NULL) {
        *data = queue->head;
        queue->head = NULL;
        status = APR_SUCCESS;
    } else {
        status = apr_queue_trypop(queue->queue, data);
    }
    apr_thread_mutex_unlock(queue->mx);
    return status;

}

static apr_status_t mpr_queue_push(mpr_queue_t *queue, void* data) {
    apr_thread_mutex_lock(queue->mx);
    apr_status_t status = apr_queue_trypush(queue->queue, data);
    apr_thread_mutex_unlock(queue->mx);
    return status;
}

static void mpr_queue_peek(mpr_queue_t *queue, void** data) {
    apr_thread_mutex_lock(queue->mx);
    apr_status_t status;
    if (queue->head != NULL) {
        *data = queue->head;
        status = APR_SUCCESS;
    } else {
        status = apr_queue_trypop(queue->queue, &queue->head);
        if (status == APR_SUCCESS) {
            *data = queue->head;
        } else {
            *data = NULL;
        }
    }
    apr_thread_mutex_unlock(queue->mx);
}


#endif	/* QUEUE_H */

