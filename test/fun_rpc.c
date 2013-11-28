#include <apr_thread_cond.h>
#include "rpc/rpc.h"

#define N_RPC (1000000)

static apr_pool_t *mp_rpc_ = NULL;
static apr_thread_cond_t *cd_rpc_ = NULL;
static apr_thread_mutex_t *mx_rpc_ = NULL;
static int ct_rpc_ = 0;

funid_t ADD = 1;
static apr_time_t tm_begin_ = 0;
static apr_time_t tm_end_ = 0;


typedef struct {
    uint32_t a;
    uint32_t b;
} struct_add;

rpc_state* add(rpc_state *in) {
    LOG_DEBUG("server received rpc request.\n");
    struct_add *sa = (struct_add *)in->buf;
    uint32_t c = sa->a + sa->b;
    rpc_state *out = (rpc_state *)malloc(sizeof(rpc_state));    
    out->buf = (uint8_t*)malloc(sizeof(uint32_t));
    out->sz = sizeof(uint32_t);
    memcpy(out->buf, &c, sizeof(uint32_t));
    return out;
}

rpc_state* add_cb(rpc_state *in) {
    // Do nothing
    LOG_DEBUG("client callback exceuted.\n");
    ct_rpc_++;
    if (ct_rpc_ == N_RPC) {
        tm_end_ = apr_time_now();
        apr_thread_mutex_lock(mx_rpc_);
        apr_thread_cond_signal(cd_rpc_);
        apr_thread_mutex_unlock(mx_rpc_);
    }
    return NULL;
}

int main() {
    apr_initialize();
    apr_pool_create(&mp_rpc_, NULL);
    apr_thread_cond_create(&cd_rpc_, mp_rpc_);
    apr_thread_mutex_create(&mx_rpc_, APR_THREAD_MUTEX_UNNESTED, mp_rpc_);

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

    apr_thread_mutex_lock(mx_rpc_);

    tm_begin_ = apr_time_now();
    for (int i = 0; i < N_RPC; i++) {
        struct_add sa;
        sa.a = 1;
        sa.b = 2;
        client_call(client, ADD, (uint8_t *)&sa, sizeof(struct_add));

    }
    printf("rpc triggered for %d adds.\n", N_RPC);

    apr_thread_cond_wait(cd_rpc_, mx_rpc_);
    apr_thread_mutex_unlock(mx_rpc_);
    int period = (tm_end_ - tm_begin_) / 1000;
    printf("finish %d rpc in %d ms.\n", N_RPC, period);

    rpc_destroy();
}

