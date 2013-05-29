#include <sys/types.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <db.h>
#include <assert.h>

#include "lock.h"
#include "operation.h"
#include "mpaxos/mpaxos.h"
#include "mpaxos/mpaxos-config.h"
#include "kvdb/kvdb.h"
#include "kvdb_log.h"

static int initilized = 0;
static DB_ENV * dbenv;
#define DBT_WRAP(dbkey, key, keylen)        \
    do {                                    \
        memset(&dbkey, 0, sizeof(dbkey));   \
        dbkey.data = key;                   \
        dbkey.size = keylen;                \
    } while (0)

static std::map<uint64_t, Operation> operations;
static std::map<groupid_t, DB *> dbs;

static int open_db(groupid_t gid) {
    if (dbs[gid] != NULL) {
        return 0;
    }
    char tmp[25];
    memset(tmp, 0, sizeof(tmp));
    sprintf(tmp, "%u.db", gid);
    int ret = db_create(&dbs[gid], dbenv, 0);
    if (ret) {
        dbs[gid] = NULL;
        dbenv->close(dbenv, 0);
        EE("db_create failed");
        return ret;
    }
    ret = dbs[gid]->open(dbs[gid], NULL, tmp, NULL, DB_BTREE, DB_CREATE, 0);
    if (ret) {
        dbs[gid] = NULL;
        dbs[gid]->err(dbs[gid], ret, "Opening: %s", tmp);
        dbenv->close(dbenv, 0);
        return ret;
    }
    return 0;
}

void mpaxos_callback(groupid_t id, slotid_t slot, uint8_t *msg, size_t mlen, void * param) {
    OperationParam * commit_param = (OperationParam *)param;
    
    DD("mpaxos callback table = %d, slot = %d, msg len = %zu, op_id = %lu", id, slot, mlen, (unsigned long)commit_param->id);

    std::map<uint64_t, Operation>::iterator it;
    it = operations.find(commit_param->id);

    Buf buf(msg, mlen);
    Operation op = unwrap(buf);

    assert(op.code < OPERATION_TYPE_COUNT && op.code >= OP_NOP);

    if (it != operations.end()) {
        it->second.lock.lock();
        it->second.code = op.code;
        it->second.args = op.args;
    }

    if (op.pairs == 1) {
        assert((uintptr_t)op.tables == id);
    }

    int rs = open_db(id);
    if (rs) {
        if (it != operations.end()) {
            it->second.result.errcode = rs;
        }
        EE("error open db for %d, op = %d", id, op.code);
    } else {
        if (op.code == OP_PUT || (op.code == OP_BATCH_PUT && op.pairs == 1)) {
            DBT dbkey;
            DBT dbval;
            DBT_WRAP(dbkey, op.args[0].buf, op.args[0].len);
            DBT_WRAP(dbval, op.args[1].buf, op.args[1].len);

            rs = dbs[id]->put(dbs[id], NULL, &dbkey, &dbval, 0);
            // TODO: sync db to disk? or just wait final destroy to flush all data?
            if (it != operations.end()) {
                it->second.result.errcode = rs;
                it->second.result.progress = rs ? -1 : 1;
            }
            if (rs) {
                EE("error put value for %d", id);
            }
        } else if (op.code == OP_GET) {
            DBT dbkey;
            DBT dbval;
            memset(&dbval, 0, sizeof(dbval));
            DBT_WRAP(dbkey, op.args[0].buf, op.args[0].len);
            rs = dbs[id]->get(dbs[id], NULL, &dbkey, &dbval, 0);
            if (rs && rs != DB_NOTFOUND) {
                EE("error get value for %d", id);
            }
            if (it != operations.end()) {
                if (rs == DB_NOTFOUND) {
                    it->second.result.errcode = KVDB_GET_KEY_NOT_EXIST;
                    it->second.result.progress = -1;
                } else {
                    it->second.result.errcode = rs;
                    it->second.result.progress = rs ? -1 : 1;
                }
                if (!rs) {
                    it->second.result.buf = (uint8_t *)dbval.data;
                    it->second.result.len = dbval.size;
                }
            }
        } else if (op.code == OP_DEL) {
            DBT dbkey;
            DBT_WRAP(dbkey, op.args[0].buf, op.args[0].len);
            rs = dbs[id]->del(dbs[id], NULL, &dbkey, 0);
            if (rs && rs != DB_NOTFOUND) {
                EE("error del value for %d", id);
            }
            if (it != operations.end()) {
                if (rs == DB_NOTFOUND) {
                    it->second.result.errcode = KVDB_DEL_KEY_NOT_EXIST;
                    it->second.result.progress = -1;
                } else {
                    it->second.result.errcode = rs;
                    it->second.result.progress = rs ? -1 : 1;
                }
            }
        } else if (op.code == OP_BATCH_PUT) {
            assert(op.pairs > 1);
            for (int p = 0; p < op.pairs; p++) {
                if (op.tables[p] != id) {
                    continue;
                }
                DBT dbkey;
                DBT dbval;
                DBT_WRAP(dbkey, op.args[2 * p].buf, op.args[2 * p].len);
                DBT_WRAP(dbval, op.args[2 * p + 1].buf, op.args[2 * p + 1].len);
                
                rs = dbs[id]->put(dbs[id], NULL, &dbkey, &dbval, 0);
                if (it != operations.end()) {
                    it->second.result.progress++;
                    it->second.result.errcode = rs;
                }
                if (rs) {
                    EE("error put value for %d in batch put", id);
                    it->second.result.progress = -1;
                    break;
                }
            }
        }
    }

    if (it != operations.end()) {
        it->second.lock.signal();
    }
    
    if (it != operations.end()) {
        it->second.lock.unlock();
    }
}

int kvdb_init(char * dbhome, char * mpaxos_config_path) {
    if (initilized) {
        WW("kvdb already initilized");
        return 0;
    }

    int ret = db_env_create(&dbenv, 0);
    II("db_env_create: %d", ret);
    if (ret) {
        EE("db_env_create failed");
        return ret;
    }
    ret = dbenv->open(dbenv, dbhome, DB_INIT_TXN | DB_INIT_MPOOL | DB_INIT_LOCK | DB_CREATE | DB_RECOVER, 0);
    if (ret) {
        dbenv->err(dbenv, ret, "Opening enviroment: %s", dbhome);
        dbenv->close(dbenv, 0);
        return ret;
    } else {
        II("dbenv->open: %d", ret);
    }

    mpaxos_init();

    mpaxos_load_config(mpaxos_config_path);

    // register mpaxos global callback
    mpaxos_set_cb_god(mpaxos_callback);

    mpaxos_start();

    initilized = 1;
    return KVDB_RET_OK;
}

int kvdb_destroy() {
    if (!initilized) {
        return KVDB_RET_UNINITIALISED;
    }

    for (auto & kv : dbs) {
        kv.second->close(kv.second, 0);
    }
    mpaxos_stop();
    mpaxos_destroy();
    return KVDB_RET_OK;
}

int kvdb_put(groupid_t table, uint8_t * key, size_t klen, uint8_t * value, size_t vlen) {
    if (!initilized) {
        return KVDB_RET_UNINITIALISED;
    }
    uint64_t op_id = genOperationId();

    operations[op_id].lock.lock();

    Buf keybuf(key, klen);
    Buf valbuf(value, vlen);
    Buf commit = wrap(OP_PUT, table, keybuf, valbuf);

    OperationParam * param = new OperationParam();
    param->id = op_id;

    II("id: %lu, PUT commit_async: table = %d, value len = %zu", op_id, table, commit.len);
    commit_async(&table, 1, commit.buf, commit.len, (void *)param);

    while (operations[op_id].result.progress >= 0 && operations[op_id].result.progress < 1) {
        operations[op_id].lock.wait();
    }
    operations[op_id].lock.unlock();

    return operations[op_id].result.errcode;
}

int kvdb_get(groupid_t table, uint8_t * key, size_t klen, uint8_t ** value, size_t *vlen) {
    if (!initilized) {
        return KVDB_RET_UNINITIALISED;
    }
    uint64_t op_id = genOperationId();

    operations[op_id].lock.lock();

    Buf keybuf(key, klen);
    Buf commit = wrap(OP_GET, table, keybuf);

    OperationParam * param = new OperationParam();
    param->id = op_id;

    II("id: %lu, GET commit_async: table = %d, value len = %zu", op_id, table, commit.len);
    commit_async(&table, 1, commit.buf, commit.len, (void *)param);

    while (operations[op_id].result.progress >= 0 && operations[op_id].result.progress < 1) {
        operations[op_id].lock.wait();
    }
    operations[op_id].lock.unlock();

    if (operations[op_id].result.errcode == 0) {
        *vlen = operations[op_id].result.len;
        *value = new uint8_t[*vlen];
        memcpy(*value, operations[op_id].result.buf, *vlen);
    }

    return operations[op_id].result.errcode;
}

int kvdb_del(groupid_t table, uint8_t * key, size_t klen) {
    if (!initilized) {
        return KVDB_RET_UNINITIALISED;
    }
    uint64_t op_id = genOperationId();

    operations[op_id].lock.lock();

    Buf keybuf(key, klen);
    Buf commit = wrap(OP_DEL, table, keybuf);

    OperationParam * param = new OperationParam();
    param->id = op_id;

    II("id: %lu, DEL commit_async: table = %d, value len = %zu", op_id, table, commit.len);
    commit_async(&table, 1, commit.buf, commit.len, (void *)param);

    while (operations[op_id].result.progress >= 0 && operations[op_id].result.progress < 1) {
        operations[op_id].lock.wait();
    }
    operations[op_id].lock.unlock();

    return operations[op_id].result.errcode;
}

int kvdb_batch_put(groupid_t * tables, uint8_t ** keys, size_t * klens, uint8_t ** vals, size_t * vlens, uint32_t pairs) {
    if (!initilized) {
        return KVDB_RET_UNINITIALISED;
    }
    uint64_t op_id = genOperationId();

    operations[op_id].lock.lock();

    // wrap batch put in one single log
    Buf * args = new Buf[pairs * 2];
    for (int p = 0; p < pairs; p++) {
        args[2 * p].buf = keys[p];
        args[2 * p].len = klens[p];
        args[2 * p + 1].buf = vals[p];
        args[2 * p + 1].len = vlens[p];
    }
    Operation op(OP_BATCH_PUT, tables, args, pairs);
    Buf commit = wrap(op);

    OperationParam * param = new OperationParam();
    param->id = op_id;

    // commit all the tables
    commit_async(tables, pairs, commit.buf, commit.len, (void *)param);

    while (operations[op_id].result.progress >= 0 && operations[op_id].result.progress < pairs) {
        DD("progress = %d", operations[op_id].result.progress);
        operations[op_id].lock.wait();
    }
    DD("finish batch put");
    operations[op_id].lock.unlock();
    return operations[op_id].result.errcode;
}

int kvdb_transaction_start() {
    // wrap operation to buffer
    // try_commit 
    return 0;
}

/* vim: set et ai ts=4 sw=4: */

