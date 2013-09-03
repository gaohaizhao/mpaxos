/*
 * mpaxos_types.h
 *
 *  Created on: Jan 29, 2013
 *      Author: ms
 */


#ifndef __MPAXOS_TYPES_H__
#define __MPAXOS_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <apr_hash.h>
#include <apr_thread_proc.h>
#include <apr_thread_cond.h>

typedef uint64_t groupid_t;
typedef uint64_t nodeid_t;
typedef uint64_t slotid_t;
typedef uint64_t ballotid_t;
typedef struct {
    uint8_t *data;
    uint32_t len;
} value_t;

typedef void(*mpaxos_cb_t)(groupid_t*, size_t, slotid_t*, uint8_t *, size_t, void *para);

#endif /* MPAXOS_TYPES_H_ */
