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

typedef uint32_t groupid_t;
typedef uint32_t nodeid_t;
typedef uint32_t slotid_t;
typedef uint32_t ballotid_t;
typedef struct {
    uint8_t *data;
    uint32_t len;
} value_t;

typedef struct {
    groupid_t* gids;
    size_t sz_gids;
    uint8_t *data;
    size_t sz_data;
    void* cb_para;
} mpaxos_req_t;

// TODO [IMPROVE]
#define mpaxos_request_t mpaxos_req_t

typedef void(*mpaxos_cb_t)(groupid_t, slotid_t, uint8_t *, size_t, void *para);

#endif /* MPAXOS_TYPES_H_ */
