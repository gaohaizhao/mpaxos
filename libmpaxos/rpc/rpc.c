#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <apr_thread_proc.h>
#include <apr_network_io.h>
#include <apr_poll.h>
#include <errno.h>
#include "rpc.h"
#include "utils/logger.h"
#include "utils/safe_assert.h"
#include "utils/mpr_thread_pool.h"
#include "utils/mpr_hash.h"

#define MAX_ON_READ_THREADS 1
#define POLLSET_NUM 1000

static apr_pool_t *mp_rpc_ = NULL; 
static server_t *server_ = NULL;
static apr_thread_t *th_poll_ = NULL;

static apr_pollset_t *pollset_ = NULL;
static int send_buf_size = 0;
static socklen_t optlen;
static int exit_ = 0;
//static apr_thread_pool_t *tp_on_read_;
static mpr_thread_pool_t *tp_read_;

// for statistics
static apr_time_t time_start_ = 0;
static apr_time_t time_last_ = 0;
static apr_time_t time_curr_ = 0;
static uint64_t sz_data_ = 0;
static uint64_t sz_last_ = 0;
static uint32_t n_data_recv_ = 0;
static uint32_t sz_data_sent_ = 0;
static uint32_t sz_data_tosend_ = 0;
static uint32_t n_data_sent_ = 0;

void rpc_init() {
    apr_initialize();
    apr_pool_create(&mp_rpc_, NULL);
    apr_pollset_create(&pollset_, POLLSET_NUM, mp_rpc_, APR_POLLSET_THREADSAFE);
    apr_thread_create(&th_poll_, NULL, start_poll, (void*)pollset_, mp_rpc_);
}

void rpc_destroy() {
    while (th_poll_ == NULL) {
        // not started.
    }
    exit_ = 1;
    LOG_DEBUG("recv server ends.");
/*
    apr_pollset_wakeup(pollset_);
*/
    apr_status_t status = APR_SUCCESS;
    apr_thread_join(&status, th_poll_);
    apr_pollset_destroy(pollset_);
    apr_pool_destroy(mp_rpc_);
    atexit(apr_terminate);
}

void client_create(client_t** c) {
    *c = (client_t *)malloc(sizeof(client_t));
    client_t *client = *c;
    apr_pool_create(&client->com.mp, NULL);
    context_t *ctx = context_gen(&(*c)->com);
    (*c)->ctx = ctx;
    mpr_hash_create(&(*c)->com.ht);

}

void client_destroy(client_t* c) {
    context_destroy(c->ctx);
    free(c);
}

void server_create(server_t** s) {
    *s = (server_t*)malloc(sizeof(server_t));
    server_t *server = *s;

    apr_status_t status = APR_SUCCESS;
    apr_pool_create(&server->com.mp, NULL);
    mpr_hash_create(&server->com.ht);
    server->sz_ctxs = 0; 
    server->ctxs = apr_pcalloc(server->com.mp, sizeof(context_t **) * 10);
}

void server_destroy(server_t *s) {
    mpr_hash_destroy(s->com.ht);
    for (int i = 0; i < s->sz_ctxs; i++) {
        context_t *ctx = s->ctxs[i];
        context_destroy(ctx);
    }
    apr_pool_destroy(s->com.mp);
}

void server_regfun(server_t *s, funid_t fid, void* fun) {
    LOG_TRACE("server regisger function, %x", fun);
    mpr_hash_set(s->com.ht, &fid, sizeof(funid_t), &fun, sizeof(void*)); 
}

void client_regfun(client_t *c, funid_t fid, void *fun) {
    LOG_TRACE("client regisger function, %x", fun);
    mpr_hash_set(c->com.ht, &fid, sizeof(funid_t), &fun, sizeof(void*)); 
}


void server_bind_listen(server_t *r) {
    apr_sockaddr_info_get(&r->com.sa, NULL, APR_INET, r->com.port, 0, r->com.mp);
/*
    apr_socket_create(&r->s, r->sa->family, SOCK_DGRAM, APR_PROTO_UDP, r->pl_recv);
*/
    apr_socket_create(&r->com.s, r->com.sa->family, SOCK_STREAM, APR_PROTO_TCP, r->com.mp);
    apr_socket_opt_set(r->com.s, APR_SO_NONBLOCK, 1);
    apr_socket_timeout_set(r->com.s, -1);
    /* this is useful for a server(socket listening) process */
    apr_socket_opt_set(r->com.s, APR_SO_REUSEADDR, 1);
    apr_socket_opt_set(r->com.s, APR_TCP_NODELAY, 1);
    
    apr_status_t status = APR_SUCCESS;
    status = apr_socket_bind(r->com.s, r->com.sa);
    if (status != APR_SUCCESS) {
        LOG_ERROR("cannot bind.");
        printf("%s", apr_strerror(status, malloc(100), 100));
        SAFE_ASSERT(status == APR_SUCCESS);
    }
    status = apr_socket_listen(r->com.s, 20);
    if (status != APR_SUCCESS) {
        LOG_ERROR("cannot listen.");
        printf("%s", apr_strerror(status, malloc(100), 100));
        SAFE_ASSERT(status == APR_SUCCESS);
    }

}

context_t *context_gen(rpc_common_t *com) {
    context_t *ctx = malloc(sizeof(context_t));
    ctx->buf_recv.buf = malloc(BUF_SIZE__);
    ctx->buf_recv.sz = BUF_SIZE__;
    ctx->buf_recv.offset_begin = 0;
    ctx->buf_recv.offset_end = 0;
    
    ctx->buf_send.sz = BUF_SIZE__;
    ctx->buf_send.buf = calloc(BUF_SIZE__, 1);
    ctx->buf_send.offset_begin = 0;
    ctx->buf_send.offset_end = 0;
    ctx->com = com;
    //ctx->on_recv = on_recv;
   
    apr_pool_create(&ctx->mp, NULL);
    apr_thread_mutex_create(&ctx->mx, APR_THREAD_MUTEX_UNNESTED, ctx->mp);
    return ctx;
}

void context_destroy(context_t *ctx) {
    apr_pool_destroy(ctx->mp);
    free(ctx->buf_recv.buf);
    free(ctx->buf_send.buf);
    free(ctx);
}

void add_write_buf_to_ctx(context_t *ctx, funid_t type, 
    const uint8_t *buf, size_t sz_buf) {
    apr_thread_mutex_lock(ctx->mx);
    
    apr_atomic_add32(&sz_data_tosend_, sizeof(funid_t) + sz_buf + sizeof(size_t));
    apr_atomic_inc32(&n_data_sent_);
    
    // realloc the write buf if not enough.
    if (sz_buf + sizeof(size_t) + sizeof(funid_t)
        > ctx->buf_send.sz - ctx->buf_send.offset_end) {
        LOG_TRACE("remalloc sending buffer.");
        uint8_t *newbuf = malloc(BUF_SIZE__);
        memcpy(newbuf, ctx->buf_send.buf + ctx->buf_send.offset_begin, 
                ctx->buf_send.offset_end - ctx->buf_send.offset_begin);
        free(ctx->buf_send.buf);
        ctx->buf_send.buf = newbuf;
        ctx->buf_send.offset_end -= ctx->buf_send.offset_begin;
        ctx->buf_send.offset_begin = 0;
        // there is possibility that even remalloc, still not enough
        if (sz_buf + sizeof(size_t) + sizeof(funid_t)
            > ctx->buf_send.sz - ctx->buf_send.offset_end) {
            LOG_ERROR("no enough write buffer after remalloc");
            SAFE_ASSERT(0);
        }
    } else {
        SAFE_ASSERT(1);
    }
    // copy memory
    LOG_TRACE("add message to sending buffer, message size: %d", sz_buf);
     
    LOG_TRACE("size in buf:%llx, original size:%llx", 
        *(ctx->buf_send.buf + ctx->buf_send.offset_end), sz_buf + sizeof(funid_t));
    
    *(size_t*)(ctx->buf_send.buf + ctx->buf_send.offset_end) = sz_buf + sizeof(funid_t);
    ctx->buf_send.offset_end += sizeof(size_t);
    memcpy(ctx->buf_send.buf + ctx->buf_send.offset_end, &type, sizeof(funid_t));
    ctx->buf_send.offset_end += sizeof(funid_t);
    memcpy(ctx->buf_send.buf + ctx->buf_send.offset_end, buf, sz_buf);
    ctx->buf_send.offset_end += sz_buf;
    
    // change poll type
    if (ctx->pfd.reqevents == APR_POLLIN) {
        apr_pollset_remove(pollset_, &ctx->pfd);
        ctx->pfd.reqevents = APR_POLLIN | APR_POLLOUT;
        apr_pollset_add(pollset_, &ctx->pfd);
    }
    
    apr_thread_mutex_unlock(ctx->mx);
}

void reply_to(read_state_t *state) {
    add_write_buf_to_ctx(state->ctx, state->reply_msg_type,
        state->buf_write, state->sz_buf_write);
    free(state->buf_write);
}

void stat_on_write(size_t sz) {
    LOG_TRACE("sent data size: %d", sz);
    sz_data_sent_ += sz;
    apr_atomic_sub32(&sz_data_tosend_, sz);
}

void on_write(context_t *ctx, const apr_pollfd_t *pfd) {
    apr_thread_mutex_lock(ctx->mx);
    LOG_TRACE("write message on socket %x", pfd->desc.s);
    apr_status_t status = APR_SUCCESS;
    uint8_t *buf = ctx->buf_send.buf + ctx->buf_send.offset_begin;
    size_t n = ctx->buf_send.offset_end - ctx->buf_send.offset_begin;
    if (n > 0) {
        status = apr_socket_send(pfd->desc.s, (char *)buf, &n);
        stat_on_write(n);
        SAFE_ASSERT(status == APR_SUCCESS);
        ctx->buf_send.offset_begin += n;
    } else {
        SAFE_ASSERT(0);
    }
    
    n = ctx->buf_send.offset_end - ctx->buf_send.offset_begin;
    if (n == 0) {
        // buf empty, remove out poll.
        apr_pollset_remove(pollset_, &ctx->pfd);
        ctx->pfd.reqevents = APR_POLLIN;
        apr_pollset_add(pollset_, &ctx->pfd);
    }
    
    apr_thread_mutex_unlock(ctx->mx);
}


/**
 * not thread-safe
 * @param sz
 */
void stat_on_read(size_t sz) {
    time_curr_ = apr_time_now();
    time_last_ = (time_last_ == 0) ? time_curr_ : time_last_;
/*
    if (time_start_ == 0) {
        time_start_ = (time_start_ == 0) ? time_curr_: time_start_;
        recv_last_time = recv_start_time;
    }
*/
    sz_data_ += sz;
    sz_last_ += sz;
    apr_time_t period = time_curr_ - time_last_;
    if (period > 1000000) {
        //uint32_t n_push;
        //uint32_t n_pop;
        //mpaxos_async_push_pop_count(&n_push, &n_pop);
        float speed = (double)sz_last_ / (1024 * 1024) / (period / 1000000.0);
        //printf("%d messages %"PRIu64" bytes received. Speed: %.2f MB/s. "
        //    "Total sent count: %d,  bytes:%d, left to send: %d",// n_push:%d, n_pop:%d\n", 
        //    apr_atomic_read32(&n_data_recv_), 
        //    sz_data_, 
        //    speed, 
        //    apr_atomic_read32(&n_data_sent_), 
        //    apr_atomic_read32(&sz_data_sent_),
        //    apr_atomic_read32(&sz_data_tosend_)); 
        //    //n_push, n_pop);
        time_last_ = time_curr_;
        sz_last_ = 0;
    }
}

void on_read(context_t * ctx, const apr_pollfd_t *pfd) {
    LOG_TRACE("HERE I AM, ON_READ");
    apr_status_t status = APR_SUCCESS;

    uint8_t *buf = ctx->buf_recv.buf + ctx->buf_recv.offset_end;
    size_t n = ctx->buf_recv.sz - ctx->buf_recv.offset_end;
    
    status = apr_socket_recv(pfd->desc.s, (char *)buf, &n);
//    LOG_DEBUG("finish reading socket.");
    if (status == APR_SUCCESS) {
        stat_on_read(n);
        ctx->buf_recv.offset_end += n;
        if (n == 0) {
            LOG_WARN("received an empty message.");
        } else {
            // LOG_DEBUG("raw data received.");
            // extract message.
            while (ctx->buf_recv.offset_end - ctx->buf_recv.offset_begin > sizeof(size_t)) {
                size_t sz_msg = *((size_t *)(ctx->buf_recv.buf + ctx->buf_recv.offset_begin));
                LOG_TRACE("next recv message size: %d", sz_msg);
                if (ctx->buf_recv.offset_end - ctx->buf_recv.offset_begin >= sz_msg + sizeof(size_t)) {
                    buf = ctx->buf_recv.buf + ctx->buf_recv.offset_begin + sizeof(size_t);
                    ctx->buf_recv.offset_begin += sz_msg + sizeof(size_t);

                    funid_t fid = *(funid_t*)(buf);
                    rpc_state *state = malloc(sizeof(rpc_state));
                    state->sz = sz_msg - sizeof(funid_t);
                    state->buf = malloc(sz_msg);
                    memcpy(state->buf, buf + sizeof(funid_t), state->sz);
//                    state->ctx = ctx;
/*
                    apr_thread_pool_push(tp_on_read_, (*(ctx->on_recv)), (void*)state, 0, NULL);
//                    mpr_thread_pool_push(tp_read_, (void*)state);
*/
                    apr_atomic_inc32(&n_data_recv_);
                    //(*(ctx->on_recv))(NULL, state);
                    // FIXME call
                    rpc_state* (**fun)(void*) = NULL;
                    size_t sz;
                    mpr_hash_get(ctx->com->ht, &fid, sizeof(funid_t), (void**)&fun, &sz);
                    SAFE_ASSERT(fun != NULL);
                    LOG_TRACE("going to call function %x", *fun);
                    rpc_state *ret_s = (**fun)(state);
                    free(state);

                    if (ret_s != NULL) {
                        add_write_buf_to_ctx(ctx, fid, ret_s->buf, ret_s->sz);
                    }
                    free(ret_s);
        
                } else {
                    break;
                }
            }

            if (ctx->buf_recv.offset_end + BUF_SIZE__ / 10 > ctx->buf_recv.sz) {
                // remalloc the buffer
                // TODO [fix] this remalloc is nothing if buf is full.
                LOG_TRACE("remalloc recv buf");
                uint8_t *buf = calloc(BUF_SIZE__, 1);
                memcpy(buf, ctx->buf_recv.buf + ctx->buf_recv.offset_begin, 
                        ctx->buf_recv.offset_end - ctx->buf_recv.offset_begin);
                free(ctx->buf_recv.buf);
                ctx->buf_recv.buf = buf;
                ctx->buf_recv.offset_end -= ctx->buf_recv.offset_begin;
                ctx->buf_recv.offset_begin = 0;
            }
        }
    } else if (status == APR_EOF) {
        LOG_WARN("received apr eof, what to do?");
        apr_pollset_remove(pollset_, &ctx->pfd);
    } else if (status == APR_ECONNRESET) {
        LOG_WARN("oops, seems that i just lost a buddy");
        // TODO [improve] you may retry connect
        apr_pollset_remove(pollset_, &ctx->pfd);
    } else {
        printf("%s\n", apr_strerror(status, malloc(100), 100));
        SAFE_ASSERT(0);
    }
}

void on_accept(server_t *r) {
    apr_status_t status = APR_SUCCESS;
    apr_socket_t *ns = NULL;
    status = apr_socket_accept(&ns, r->com.s, r->com.mp);
    if (status != APR_SUCCESS) {
        LOG_ERROR("recvr accept error.");
        LOG_ERROR("%s", apr_strerror(status, calloc(100, 1), 100));
        SAFE_ASSERT(status == APR_SUCCESS);
    }
    apr_socket_opt_set(ns, APR_SO_NONBLOCK, 1);
    apr_socket_opt_set(ns, APR_TCP_NODELAY, 1);
    context_t *ctx = context_gen(&r->com);
    ctx->s = ns;
    apr_pollfd_t pfd = {ctx->mp, APR_POLL_SOCKET, APR_POLLIN, 0, {NULL}, NULL};
    ctx->pfd = pfd;
    ctx->pfd.desc.s = ns;
    ctx->pfd.client_data = ctx;
    r->ctxs[r->sz_ctxs++] = ctx;
    apr_pollset_add(pollset_, &ctx->pfd);
}

void* APR_THREAD_FUNC start_poll(apr_thread_t *t, void *arg) {
/*
    apr_pollset_t *pollset = arg;
*/
    apr_status_t status = APR_SUCCESS;
    while (!exit_) {
        int num = 0;
        const apr_pollfd_t *ret_pfd;
        status = apr_pollset_poll(pollset_, 100 * 1000, &num, &ret_pfd);
        if (status == APR_SUCCESS) {
            if (num <=0 ) {
                LOG_ERROR("poll error. poll num le 0.");
                SAFE_ASSERT(num > 0);
            }
            for(int i = 0; i < num; i++) {
                if (ret_pfd[i].rtnevents & APR_POLLIN) {
                    if(ret_pfd[i].desc.s == server_->com.s) {
                        // new connection arrives.
                        on_accept(server_);
                    } else {
                        on_read(ret_pfd[i].client_data, &ret_pfd[i]);
                    }
                } 
                if (ret_pfd[i].rtnevents & APR_POLLOUT) {
                    on_write(ret_pfd[i].client_data, &ret_pfd[i]);
                }
                if (!(ret_pfd[i].rtnevents | APR_POLLOUT | APR_POLLIN)) {
                    // have no idea.
                    LOG_ERROR("see some poll event neither in or out. event:%d",
                        ret_pfd[i].rtnevents);
                    SAFE_ASSERT(0);
                }
            }
        } else if (status == APR_EINTR) {
            // the signal we get when process exit, wakeup, or add in and write.
            LOG_DEBUG("the receiver epoll exits?");
            continue;
        } else if (status == APR_TIMEUP) {
            // debug.
            //int c = mpr_thread_pool_task_count(tp_read_);
/*
            LOG_INFO("epoll timeout. thread pool task size: %d", c);
*/
            stat_on_read(0);
            continue;
        } else {
            LOG_ERROR("poll error. %s", apr_strerror(status, calloc(1, 100), 100));
            SAFE_ASSERT(0);
        }
    }
    SAFE_ASSERT(apr_thread_exit(t, APR_SUCCESS) == APR_SUCCESS);
    return NULL;
}

void server_start(server_t* s) {
    server_bind_listen(s);
    server_ = s;
    apr_pollfd_t pfd = {s->com.mp, APR_POLL_SOCKET, APR_POLLIN, 0, {NULL}, NULL};
    pfd.desc.s = s->com.s;
    apr_pollset_add(pollset_, &pfd);
}

void client_connect(client_t *c) {
    LOG_DEBUG("connecting to server %s %d", c->com.ip, c->com.port);
    
    apr_sockaddr_info_get(&c->com.sa, c->com.ip, APR_INET, c->com.port, 0, c->com.mp);
    // apr_socket_create(&s->s, s->sa->family, SOCK_DGRAM, APR_PROTO_UDP, ctx->mp);
    apr_socket_create(&c->com.s, c->com.sa->family, SOCK_STREAM, APR_PROTO_TCP, c->com.mp);
    apr_socket_opt_set(c->com.s, APR_SO_NONBLOCK, 1);
    // apr_socket_opt_set(s->s, APR_SO_REUSEADDR, 1);/* this is useful for a server(socket listening) process */
    apr_socket_opt_set(c->com.s, APR_TCP_NODELAY, 1);

    //printf("Default sending buffer size %d.\n", send_buf_size);
    apr_status_t status = APR_SUCCESS;
    do {
/*
        printf("TCP CLIENT TRYING TO CONNECT.");
*/
        status = apr_socket_connect(c->com.s, c->com.sa);
    } while (status != APR_SUCCESS);
    LOG_DEBUG("connected socket on remote addr %s, port %d", c->com.ip, c->com.port);
    
    // add to epoll
    while (pollset_ == NULL) {
        // not inited yet, just wait.
    }
    context_t *ctx = c->ctx;
    ctx->s = c->com.s;
    apr_pollfd_t pfd = {ctx->mp, APR_POLL_SOCKET, APR_POLLIN, 0, {NULL}, NULL};
    ctx->pfd = pfd;
    ctx->pfd.desc.s = ctx->s;
    ctx->pfd.client_data = ctx;
    status = apr_pollset_add(pollset_, &ctx->pfd);
    SAFE_ASSERT(status == APR_SUCCESS);
}

void client_call(client_t *c, funid_t fid, const uint8_t *buf, size_t sz_buf) {
    add_write_buf_to_ctx(c->ctx, fid, buf, sz_buf);
}
