#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "operation.h"
#include "lock.h"

#include "kvdb_log.h"

static pthread_mutex_t operation_id_lock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t operation_id = 0;

uint64_t genOperationId() {
    ScopeLock sl(&operation_id_lock);
    return operation_id++;
}

// use readint and writeint to avoid endianness problem
#define readint(buf, base) (((buf[base+3]<<24)&0xff000000)| \
                           ((buf[base+2]<<16)&0xff0000)| \
                           ((buf[base+1]<<8)&0xff00)| \
                    (buf[base]&0xff))

#define writeint(buf, base, val) do{ buf[base+3]=((val)>>24)&0xff; \
                                     buf[base+2]=((val)>>16)&0xff; \
                                     buf[base+1]=((val)>>8)&0xff; \
                                     buf[base]=(val)&0xff; \
                                 }while(0)


// op | pair | [table, key [, value]] x pair

Buf wrap(Operation op) {
    Buf ret;

    if (op.code < OP_NOP || op.code >= OPERATION_TYPE_COUNT) {
        EE("invalid operation: %d", op.code);
        throw -1;
    }

    ret.len = sizeof(int) + sizeof(uint32_t);       // opcode and pairs

    for (int p = 0; p < op.pairs; p++) {
        ret.len += sizeof(uint32_t);                // table
        for(int i = 0; i < op.arg_num(); i++) {
            // note that only store 32bit here, not the full length of size_t(8 bytes sometimes)
            // so only support less than 4G content
            ret.len += sizeof(uint32_t) + op.args[p * op.arg_num() + i].len;
        }
    }
    
    ret.buf = new uint8_t[ret.len];
    size_t pos = 0;
    writeint(ret.buf, pos, op.code);
    pos += sizeof(int);

    writeint(ret.buf, pos, op.pairs);
    pos += sizeof(uint32_t);

    for (int p = 0; p < op.pairs; p++) {
        if (op.pairs == 1) {
            writeint(ret.buf, pos, (uintptr_t)(op.tables));
        } else {
            writeint(ret.buf, pos, op.tables[p]);
        }
        pos += sizeof(uint32_t);

        for (int i = 0; i < op.arg_num(); i++) {
            int index = p * op.arg_num() + i;
            writeint(ret.buf, pos, op.args[index].len);
            pos += sizeof(uint32_t);

            memcpy(ret.buf + pos, op.args[index].buf, op.args[index].len);
            pos += op.args[index].len;
        }
    }
    return ret;
}

Buf wrap(OperationCode opcode, uint32_t table, Buf a) {
    return wrap(Operation(opcode, table, a));
}

Buf wrap(OperationCode opcode, uint32_t table, Buf a, Buf b) {
    return wrap(Operation(opcode, table, a, b));
}

Operation unwrap(Buf msg) {
    Operation ret;
    size_t offset = 0;

    int opcode = readint(msg.buf, offset);
    offset += sizeof(int);

    if (opcode < OP_NOP || opcode >= OPERATION_TYPE_COUNT) {
        EE("invalid operation: %d", opcode);
        throw -1;
    }
    ret.code = (OperationCode)opcode;
    
    ret.pairs = readint(msg.buf, offset);
    offset += sizeof(uint32_t);

    if (ret.pairs <= 0) {
        throw "invalid pairs we read";
    }

    int arg_count = OPERATION_TYPES[opcode].arg_num;
    ret.args = (Buf *)malloc(sizeof(Buf) * arg_count * ret.pairs);

    if (ret.pairs > 1) {
        ret.tables = new uint64_t[ret.pairs];
    }
    for (int p = 0; p < ret.pairs; p++) {
        if (ret.pairs > 1) {
            ret.tables[p] = readint(msg.buf, offset);
        } else {
            ret.tables = (uint64_t *)(uintptr_t)readint(msg.buf, offset);
        }
        offset += sizeof(uint32_t);

        for (int i = 0; i < arg_count; i++) {
            int index = p * arg_count + i;
            ret.args[index].len = readint(msg.buf, offset);
            offset += sizeof(int);

            ret.args[index].buf = msg.buf + offset;
            offset += ret.args[index].len;
        }
    }

    DD("op: name = %s, id = %d, table = %lx, pairs = %d", ret.name(), opcode, (uintptr_t)ret.tables, ret.pairs);

    return ret;
}

