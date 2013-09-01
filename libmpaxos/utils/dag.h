/* 
 * File:   dag.h
 * Author: ms
 *
 * Created on August 20, 2013, 4:13 PM
 */

#ifndef DAG_H
#define	DAG_H

#include <apr.h>
#include <apr_hash.h>
#include <apr_queue.h>

#include "safe_assert.h"

typedef uint64_t queueid_t;
#define GRAY 0
#define WHITE 1
#define BLACK 2

typedef struct {
    apr_pool_t *pl;
    apr_queue_t *qu;
    apr_hash_t *ht;
    apr_thread_mutex_t *mx;
} mpr_dag_t;

typedef struct {
    queueid_t* qids;
    size_t sz_qids;
    int color;
    void *data;
} mpr_dag_node_t;

static void mpr_dag_create(mpr_dag_t **pp_dag) {
    *pp_dag = malloc(sizeof(mpr_dag_t));
    mpr_dag_t *d = *pp_dag;
    apr_pool_create(&d->pl, NULL);
    d->ht = apr_hash_make(d->pl);
    d->qu = apr_queue_create(&d->qu, 10000, d->pl);
    apr_thread_mutex_create(d->mx, APR_THREAD_MUTEX_UNNESTED, d->pl);
}

static void mpr_dag_destroy(mpr_dag_t *dag) {
    apr_queue_term(dag->qu);
    apr_pool_destroy(dag);
    free(dag);
}

static void mpr_dag_push(mpr_dag_t *dag, queueid_t *qids, size_t sz_qids, void* data) {
    apr_thread_mutex_lock(dag->mx);
    mpr_dag_node_t *node = malloc(sizeof(mpr_dag_node_t));
    node->color = BLACK;
    node->data = data;
    node->qids = malloc(sizeof(queueid_t) * sz_qids);
    memcpy(node->qids, qids, sizeof(queueid_t) * sz_qids);
    node->sz_qids = sz_qids;
    
    int goto_white = 1;
    for (int i = 0; i < sz_qids; i++) {
        queueid_t qid = qids[i];
        mpr_queue_t* qu = apr_hash_get(dag->ht, &qid, sizeof(queueid_t));
        if (qu == NULL) {
            mpr_queue_create(qu, 10000, dag->pl);
            void *key = malloc(sizeof(qid));
            *key = qid;
            apr_hash_set(dag->ht, key, sizeof(qid));
        }
        mpr_queue_push(qu, node);
        // peek, if all in the head, then goes into white queue.
        void *head = NULL;
        mpr_queue_peek(qu, &head);
        if (head != node) {
            goto_white = 0;
        }
    }
    if (goto_white) {
        node->color = WHITE;
        apr_queue_push(dag->qu, node);
    }
    apr_thread_mutex_unlock(dag->mx);

}

void mpr_dag_pop(mpr_dag_t *dag, queueid_t *qids, size_t sz_qids, void** data) {
apr_thread_mutex_lock(dag->mx);

    // 1. pop it from the queue
    mpr_dag_node_t *ret_node = NULL;
    mpr_dag_node_t *node = NULL;
    for (int i = 0; i < sz_qids; i++) {
        queueid_t qid = qids[i];
        mpr_queue_t* qu = apr_hash_get(dag->ht, &qid, sizeof(queueid_t));
        SAFE_ASSERT(qu != NULL);
        apr_status_t status = mpr_queue_trypop(qu, &node);
        SAFE_ASSERT(status == APR_SUCCESS);
        SAFE_ASSERT(node != NULL);
        if (ret_node != NULL) {
            SAFE_ASSERT(node == ret_node);
        } else {
            ret_node = node;
        }
        
        // 2. add the next to white queue.
        mpr_queue_peek(qu, &node);
        if (node != NULL && node->color != WHITE) {
            int goto_white = 1;
            for (int j = 0; j < node->sz_qids; j++) {
                qid = node->qids[j];
                mpr_queue_t* qu = apr_hash_get(dag->ht, &qid, sizeof(queueid_t));
                void *head = NULL;
                mpr_queue_peek(qu, &head);
                if (head != node) {
                    goto_white = 0;
                }
            }
            if (goto_white) {
                node->color = WHITE;
                apr_queue_push(dag->qu, node);
            }
        }
    }

    *data = ret_node->data;
    free(ret_node->qids);
    free(ret_node);
    apr_thread_mutex_unlock(dag->mx);
}

/**
 * must popwhite, then pop.
 * @param dag
 * @param qids
 * @param sz_qids
 * @param data
 */
apr_status_t mpr_dag_getwhite(mpr_dag_t *dag, queueid_t **qids, size_t* sz_qids, void** data) {
//    apr_thread_mutex_lock(dag->mx);
    apr_status_t status;
    mpr_dag_node_t *node = NULL;
    status = apr_queue_pop(dag->qu, &node);
    if (node == NULL) {
        *data = NULL;
    } else {
        node->color = GRAY;
        *data = node.data;
    }
    return status;
//    apr_thread_mutex_unlock(dag->mx);
}

#endif	/* DAG_H */