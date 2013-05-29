#ifndef __KVDB_H__
#define __KVDB_H__

#include "mpaxos/mpaxos-types.h"

#define KVDB_RET_OK 0
#define KVDB_RET_UNINITIALISED -1
#define KVDB_GET_KEY_NOT_EXIST 200
#define KVDB_DEL_KEY_NOT_EXIST 300

#ifdef __cplusplus
extern "C" {
#endif

int kvdb_init(char * dbhome, char * mpaxos_config_path);
int kvdb_put(groupid_t table, uint8_t *, size_t, uint8_t *, size_t);
int kvdb_get(groupid_t table, uint8_t * key, size_t klen, uint8_t ** value, size_t *vlen);
int kvdb_del(groupid_t table, uint8_t * key, size_t klen);
int kvdb_batch_put(groupid_t * tables, uint8_t ** keys, size_t * klens, uint8_t ** vals, size_t * vlens, uint32_t pairs);
int kvdb_destroy();

#ifdef __cplusplus
}
#endif

#endif

