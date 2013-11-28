#include "rpc/rpc.h"

funid_t ADD = 1;

typedef struct {
    uint32_t a;
    uint32_t b;
} struct_add;

rpc_state* add(rpc_state *in) {
    struct_add *sa = (struct_add *)in->buf;
    uint32_t c = sa->a + sa->b;
    rpc_state *out = (rpc_state *)malloc(sizeof(rpc_state));    
    out->buf = (uint8_t*)malloc(sizeof(uint32_t));
    out->sz = sizeof(uint32_t);
    memcpy(out->buf, &c, sizeof(uint32_t));
    return out;
}

void add_cb(rpc_state *in) {
    // Do nothing
}

START_TEST (rpc) {
    server_t *server = NULL;
    client_t *client = NULL;

    rpc_init();
    server_create(&server);    
    strcpy(server->com.ip, "127.0.0.1");
    server->com.port = 9999;
    server_regfun(server, ADD, add); 
    server_start(server);
    printf("server started.\n");

    client_create(&client);
    strcpy(client->com.ip, "127.0.0.1");
    client->com.port = 9999;
    client_regfun(client, ADD, add_cb);
    client_connect(client);
    printf("client connected.\n");

    struct_add sa;
    sa.a = 1;
    sa.b = 2;
    client_call(client, ADD, (uint8_t *)&sa, sizeof(struct_add));
    rpc_destroy();
}
END_TEST

