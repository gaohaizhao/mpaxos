#ifndef __OPERATION_H__
#define __OPERATION_H__

#include <cstdio>
#include "lock.h"

class Buf {
public:
    uint8_t * buf;
    size_t len;

    Buf(){};

    Buf(uint8_t * b, size_t l) : buf(b), len(l) { }

    void print() {
        for (uint32_t i = 0; i < this->len; i++) {
            putchar(this->buf[i]);
        }
    }

    void dump(const char * file) {
        FILE * f = fopen(file ? file : "buf.dump", "wb");
        fwrite(buf, len, 1, f);
        fclose(f);
    }
};

uint64_t genOperationId();

struct OperationParam {
    uint64_t id;            // operation id
};

struct OperationResult {
    uint8_t * buf;
    size_t len;
    int errcode;
    int progress;
};

enum OperationCode : unsigned int {
    OP_NOP = 0,
    OP_PUT = 1,
    OP_GET,
    OP_DEL,
    OP_BATCH_PUT
};

struct OperationType {
    OperationCode code;
    short arg_num;              // -1 for variable length
    short result_num;           // -1 for variable length
    const char * name;
};

static OperationType OPERATION_TYPES[] = {
    {OP_NOP, 0, 0, const_cast<const char *>("NOP")},
    {OP_PUT, 2, 0, const_cast<const char *>("PUT")},
    {OP_GET, 1, 1, const_cast<const char *>("GET")},
    {OP_DEL, 1, 0, const_cast<const char *>("DEL")},
    {OP_BATCH_PUT, 2, 0, const_cast<const char *>("BATCH_PUT")}
};

static int OPERATION_TYPE_COUNT = sizeof(OPERATION_TYPES)/sizeof(OperationType);

class Operation {
public:
    OperationCode code;
    Buf * args;
    OperationResult result;
    Lock lock;
    uint32_t * tables;
    uint32_t pairs;
    
    Operation() : code(OP_NOP), args(NULL), tables(NULL), pairs(0) {
        result.progress = 0;
        result.errcode = 0;
        result.buf = NULL;
        result.len = 0;
    };
    //Operation(OperationCode c, Buf * b, OperationResult r) : code(c), args(b), result(r) { }
    Operation(OperationCode op, uint32_t table, Buf key) {          // single param operation like get, del
        code = op;
        args = new Buf[1];
        args[0] = key;
        pairs = 1;
        tables = (uint32_t *)(uintptr_t)table;                 // just use tables to store the table when only one table
    }
    Operation(OperationCode opcode, uint32_t table, Buf key, Buf val) {         // 2 params operation like put
        code = opcode;
        args = new Buf[2];
        args[0] = key;
        args[1] = val;
        pairs = 1;
        tables = (uint32_t *)(uintptr_t)table;
        //printf("tables = %lx\n", (uintptr_t)tables);
    }
    Operation(OperationCode opcode, uint32_t * tableids, Buf * op_args, uint32_t pair_num) { 
        code = opcode;
        args = op_args;         // element count: pair_num x arg_num 
        pairs = pair_num;
        tables = tableids;
    }
    
    const char * name() {
        return OPERATION_TYPES[code].name;
    }

    const short arg_num() {
        return OPERATION_TYPES[code].arg_num;
    }

    const short result_num() {
        return OPERATION_TYPES[code].result_num;
    }
};

Buf wrap(Operation);
Buf wrap(OperationCode op, uint32_t table, Buf);
Buf wrap(OperationCode op, uint32_t table, Buf, Buf);
Operation unwrap(Buf);

#endif

