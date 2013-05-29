#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdint.h>
#include "operation.h"

int main() {
    Buf key;
    key.buf = (uint8_t *)"key key key";
    key.len = strlen((const char *)key.buf);

    Buf val;
    val.buf = (uint8_t *)"value value";
    val.len = strlen((const char *)val.buf);

    Buf ret = wrap(OP_PUT, 5, key, val);

    FILE * f = fopen("output.bin", "wb");
    fwrite(ret.buf, ret.len, 1, f);
    fclose(f);

    Operation op = unwrap(ret);
    printf("opname = %s, arg num = %d\n", op.name(), op.arg_num());
    int i;
    for (i = 0; i < op.arg_num(); i++) {
        op.args[i].print();
        printf("\n");
    }
    return 0;
}

