/**
 * compile with: gcc test_aprnetwork.c `pkg-config --cflags --libs apr-1 apr-util-1` --std=c99 -g -O0 -o test_aprnetwork.out
 */

#include <stdio.h>
#include <apr_thread_proc.h>
#include <apr_network_io.h>
#include <assert.h>

#define MSG_COUNT 10
#define MSG_SIZE 100
#define PORT1 9000
#define PORT2 9001
#define HOST "127.0.0.1"

static apr_pool_t *pl_;
static char buf_send_[MSG_SIZE];

/*
void assert(int expr) {
    if (!expr) {
        fflush(stdout);
        assert(0);
        exit(0);
    }
    
}
*/

void tcp_cs_server() {
    
}

void tcp_cs_client() {
    
}

void* APR_THREAD_FUNC sctp_ss_server1(apr_thread_t *th, void* arg) {
    apr_status_t status;
    apr_socket_t *s1;
    apr_sockaddr_t *sa1;
    apr_sockaddr_info_get(&sa1, NULL, APR_INET, PORT1, 0, pl_);
    apr_socket_create(&s1, sa1->family, SOCK_SEQPACKET, APR_PROTO_SCTP, pl_);
    apr_socket_opt_set(s1, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(s1, APR_TCP_NODELAY, 1);

    status = apr_socket_bind(s1, sa1);
    status = apr_socket_listen(s1, 10);

    apr_socket_t *s2;
    apr_sockaddr_t *sa2;
    apr_sockaddr_info_get(&sa2, HOST, APR_INET, PORT2, 0, pl_);
    apr_socket_create(&s2, sa1->family, SOCK_SEQPACKET, APR_PROTO_SCTP, pl_);
    apr_socket_opt_set(s2, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(s2, APR_TCP_NODELAY, 1);
    
    
    apr_sockaddr_t remote_sa;
    char buf[MSG_SIZE];
    apr_size_t n = MSG_SIZE;
    for (int i = 0; i < MSG_COUNT; i++) {
        printf("SEVER 1 ROUND START %d\n", i);
        status = apr_socket_recvfrom(&remote_sa, s1, 0, (char *)buf, &n);
        assert(status == APR_SUCCESS);
        assert(n = MSG_SIZE);
        printf("SEVER 1 ROUND %d MESSAGE RECEIVED.\n", i);
        
        status = apr_socket_sendto(s2, sa2, 0, (const char *)buf_send_, &n);
        assert(status == APR_SUCCESS);
        assert(n = MSG_SIZE);

    }
    apr_socket_close(s1);
    apr_socket_close(s2);
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}


void* APR_THREAD_FUNC sctp_ss_server2(apr_thread_t *th, void* arg) {
    apr_status_t status;
    apr_socket_t *s2;
    apr_sockaddr_t *sa2;
    apr_sockaddr_info_get(&sa2, NULL, APR_INET, PORT2, 0, pl_);
    apr_socket_create(&s2, sa2->family, SOCK_SEQPACKET, APR_PROTO_SCTP, pl_);
    apr_socket_opt_set(s2, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(s2, APR_TCP_NODELAY, 1);

    status = apr_socket_bind(s2, sa2);
    status = apr_socket_listen(s2, 10);

    apr_socket_t *s1;
    apr_sockaddr_t *sa1;
    apr_sockaddr_info_get(&sa1, HOST, APR_INET, PORT1, 0, pl_);
    apr_socket_create(&s1, sa2->family, SOCK_SEQPACKET, APR_PROTO_SCTP, pl_);
    apr_socket_opt_set(s1, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(s1, APR_TCP_NODELAY, 1);
    
    
    apr_sockaddr_t remote_sa;
    char buf[MSG_SIZE];
    apr_size_t n = MSG_SIZE;
    for (int i = 0; i < MSG_COUNT; i++) {
        status = apr_socket_sendto(s1, sa1, 0, (const char *)buf_send_, &n);
        assert(status == APR_SUCCESS);
        assert(n = MSG_SIZE);

        status = apr_socket_recvfrom(&remote_sa, s2, 0, (char *)buf, &n);
        assert(status == APR_SUCCESS);
        assert(n = MSG_SIZE);
    }
    apr_socket_close(s1);
    apr_socket_close(s2);
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}

void* APR_THREAD_FUNC sctp_tcp_cs_client(apr_thread_t *th, void* arg) {
    apr_status_t status;
    apr_socket_t *s;
    apr_sockaddr_t *sa;
    apr_sockaddr_info_get(&sa, HOST, APR_INET, PORT1, 0, pl_);
    apr_socket_create(&s, sa->family, SOCK_STREAM, APR_PROTO_SCTP, pl_);
    apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(s, APR_TCP_NODELAY, 1);

    do {
        printf("TCP CLIENT TRYING TO CONNECT.");
        status = apr_socket_connect(s, sa);
    } while (status != APR_SUCCESS);
    assert(status == APR_SUCCESS);
    printf("TCP CLIENT SUCCESSFULLY CONNECTED.\n");
    
    apr_int32_t on;
    
    
    
    apr_socket_opt_get(s, APR_SO_SNDBUF, &on);
    printf("CLIENT SEND BUF SIZE: %d\n", on);
    
    apr_socket_opt_set(s, APR_SO_SNDBUF, 10240);
    
    char buf[MSG_SIZE];
    apr_size_t n = MSG_SIZE;
    for (int i = 0; i < MSG_COUNT; i++) {
        printf("SCTP TCP CLINET ROUND %d\n", i);
        status = apr_socket_send(s, (const char *)buf_send_, &n);
        assert(status == APR_SUCCESS);
        assert(n == MSG_SIZE);
        
        status = apr_socket_send(s, (const char *)buf_send_, &n);
        assert(status == APR_SUCCESS);
        assert(n == MSG_SIZE);
        
        status = apr_socket_recv(s, (char *)buf, &n);
        assert(status == APR_SUCCESS);
        assert(n == MSG_SIZE);
    }
    apr_socket_close(s);
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}


void* APR_THREAD_FUNC sctp_tcp_cs_server(apr_thread_t *th, void* arg) {
    apr_status_t status;
    apr_socket_t *s;
    apr_sockaddr_t *sa;
    apr_sockaddr_info_get(&sa, NULL, APR_INET, PORT1, 0, pl_);
    apr_socket_create(&s, sa->family, SOCK_STREAM, APR_PROTO_SCTP, pl_);
    apr_socket_opt_set(s, APR_SO_REUSEADDR, 1);

    status = apr_socket_bind(s, sa);
    if (status != APR_SUCCESS) {
        printf(apr_strerror(status, calloc(100, 1), 100));
        assert(0);
    }
    status = apr_socket_listen(s, 10);
    assert(status == APR_SUCCESS);

    apr_sockaddr_t remote_sa;
    char buf[MSG_SIZE];
    apr_size_t n = MSG_SIZE;
    
    apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(s, APR_TCP_NODELAY, 1);
    
    apr_int32_t on;
    apr_socket_opt_get(s, APR_SO_SNDBUF, &on);
    printf("SEVER_SEND_BUF SIZE: %d\n", on);
    
    apr_socket_opt_set(s, APR_SO_SNDBUF, 10240);
    
    apr_socket_t *ns;
    status = apr_socket_accept(&ns, s, pl_);
    assert(status == APR_SUCCESS);
    
    apr_socket_opt_set(ns, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(ns, APR_TCP_NODELAY, 1);
    apr_socket_opt_set(ns, APR_SO_SNDBUF, 10240);
    
    printf("I'm here 3!\n");
    for (int i = 0; i < MSG_COUNT; i++) {
        printf("SCP TCP SEVER ROUND START %d\n", i);
        
        status = apr_socket_recv(ns, (char *)buf, &n);
        assert(status == APR_SUCCESS);
        assert(n == MSG_SIZE);
        
        status = apr_socket_recv(ns, (char *)buf, &n);
        assert(status == APR_SUCCESS);
        assert(n == MSG_SIZE);
        
        // check if they are sent from on port;
        printf("PORT %d\n", remote_sa.port);
        apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
        printf("SEVER ROUND %d MESSAGE RECEIVED.\n", i);
        status = apr_socket_send(ns, (const char *)buf_send_, &n);
        assert(status == APR_SUCCESS);
        assert(n == MSG_SIZE);
    }
    printf("SCP TCP SEVER STOP.\n");
    apr_socket_close(ns);
    apr_socket_close(s);
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}


void* APR_THREAD_FUNC sctp_cs_server(apr_thread_t *th, void* arg) {
    apr_status_t status;
    apr_socket_t *s;
    apr_sockaddr_t *sa;
    apr_sockaddr_info_get(&sa, NULL, APR_INET, PORT1, 0, pl_);
    apr_socket_create(&s, sa->family, SOCK_SEQPACKET, APR_PROTO_SCTP, pl_);
    apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(s, APR_TCP_NODELAY, 1);

    status = apr_socket_bind(s, sa);
    status = apr_socket_listen(s, 10);

    apr_sockaddr_t remote_sa;
    char buf[MSG_SIZE];
    apr_size_t n = MSG_SIZE;
    for (int i = 0; i < MSG_COUNT * 2; i++) {
        printf("SEVER ROUND START %d\n", i);
        apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
        status = apr_socket_recvfrom(&remote_sa, s, 0, (char *)buf, &n);
        assert(status == APR_SUCCESS);
        assert(n = MSG_SIZE);
        
        // check if they are sent from on port;
        printf("PORT %d\n", remote_sa.port);
        apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
        printf("SEVER ROUND %d MESSAGE RECEIVED.\n", i);
        status = apr_socket_sendto(s, &remote_sa, 0, (const char *)buf_send_, &n);
        assert(status == APR_SUCCESS);
        assert(n = MSG_SIZE);
    }
    apr_socket_close(s);
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}

void* APR_THREAD_FUNC sctp_cs_client(apr_thread_t *th, void* arg) {
    apr_status_t status;
    apr_socket_t *s;
    apr_sockaddr_t *sa;
    apr_sockaddr_info_get(&sa, HOST, APR_INET, PORT1, 0, pl_);
    apr_socket_create(&s, sa->family, SOCK_SEQPACKET, APR_PROTO_SCTP, pl_);
    apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
    apr_socket_opt_set(s, APR_TCP_NODELAY, 1);

    apr_sockaddr_t remote_sa;
    char buf[MSG_SIZE];
    apr_size_t n = MSG_SIZE;
    for (int i = 0; i < MSG_COUNT; i++) {
        printf("CLINET ROUND %d\n", i);
        apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
        status = apr_socket_sendto(s, sa, 0, (const char *)buf_send_, &n);
        assert(status == APR_SUCCESS);
        assert(n = MSG_SIZE);
        apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);
        status = apr_socket_recvfrom(&remote_sa, s, 0, (char *)buf, &n);
        assert(status == APR_SUCCESS);
        assert(n = MSG_SIZE);
    }
/*
    apr_socket_close(s);
*/
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}


void* APR_THREAD_FUNC udp_cs_server(apr_thread_t *th, void* arg) {
    apr_status_t status;
    apr_socket_t *s;
    apr_sockaddr_t *sa;
    apr_sockaddr_info_get(&sa, NULL, APR_INET, PORT1, 0, pl_);
    apr_socket_create(&s, sa->family, SOCK_DGRAM, APR_PROTO_UDP, pl_);
    apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);

    status = apr_socket_bind(s, sa);

    apr_sockaddr_t remote_sa;
    char buf[MSG_SIZE];
    apr_size_t n = MSG_SIZE;
    for (int i = 0; i < MSG_COUNT; i++) {
        printf("SEVER ROUND START %d\n", i);
        status = apr_socket_recvfrom(&remote_sa, s, 0, (char *)buf, &n);
        printf("SEVER ROUND %d MESSAGE RECEIVED.\n", i);
        apr_socket_sendto(s, &remote_sa, 0, (const char *)buf_send_, &n);
    }
    apr_socket_close(s);
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}

void* APR_THREAD_FUNC udp_cs_client(apr_thread_t *th, void* arg) {
    apr_status_t status;
    apr_socket_t *s;
    apr_sockaddr_t *sa;
    apr_sockaddr_info_get(&sa, HOST, APR_INET, PORT1, 0, pl_);
    apr_socket_create(&s, sa->family, SOCK_DGRAM, APR_PROTO_UDP, pl_);
    apr_socket_opt_set(s, APR_SO_NONBLOCK, 0);

    apr_sockaddr_t remote_sa;
    char buf[MSG_SIZE];
    apr_size_t n = MSG_SIZE;
    for (int i = 0; i < MSG_COUNT; i++) {
        printf("CLINET ROUND %d\n", i);
        apr_socket_sendto(s, sa, 0, (const char *)buf_send_, &n);
        status = apr_socket_recvfrom(&remote_sa, s, 0, (char *)buf, &n);
    }
    apr_socket_close(s);
    apr_thread_exit(th, APR_SUCCESS);
    return NULL;
}

void test_sctp_ss() {
    apr_thread_t *th1;
    apr_thread_t *th2;
    apr_thread_create(&th1, NULL, sctp_ss_server1, NULL, pl_);
    sleep(1);
    apr_time_t t1 = apr_time_now();
    apr_thread_create(&th2, NULL, sctp_ss_server2, NULL, pl_);
    apr_status_t status;
    apr_thread_join(&status, th1);
    apr_thread_join(&status, th2);
    apr_time_t t2 = apr_time_now();
    double msg = MSG_COUNT * MSG_SIZE;
    int period = t2 - t1;
    double rate = (MSG_COUNT * 1000000.0) / period;
    printf("FINISHED SCTP SS TEST IN %d microsencds, RATE: %.2f MSG PER SECOND, THROUGHPUT: %.2fMB/s\n", period, rate, msg / period);
}

void test_sctp_cs() {
    apr_thread_t *th1;
    apr_thread_t *th2;
    apr_thread_t *th3;
    apr_thread_create(&th1, NULL, sctp_cs_server, NULL, pl_);
    sleep(1);
    apr_time_t t1 = apr_time_now();
    apr_thread_create(&th2, NULL, sctp_cs_client, NULL, pl_);
    apr_thread_create(&th3, NULL, sctp_cs_client, NULL, pl_);
    apr_status_t status;
    apr_thread_join(&status, th1);
    apr_thread_join(&status, th2);
    apr_thread_join(&status, th3);
    apr_time_t t2 = apr_time_now();
    double msg = MSG_COUNT * MSG_SIZE * 2;
    int period = t2 - t1;
    double rate = (MSG_COUNT * 2 * 1000000.0) / period;
    printf("FINISHED SCTP CS TEST IN %d microsencds, RATE: %.2f MSG PER SECOND, THROUGHPUT: %.2fMB/s\n", period, rate, msg / period);
}

void test_sctp_tcp_cs() {
    apr_thread_t *th1;
    apr_thread_t *th2;
    apr_thread_t *th3;
    apr_thread_create(&th1, NULL, sctp_tcp_cs_server, NULL, pl_);
    sleep(2);
    apr_time_t t1 = apr_time_now();
    apr_thread_create(&th2, NULL, sctp_tcp_cs_client, NULL, pl_);
/*
    apr_thread_create(&th3, NULL, sctp_cs_client, NULL, pl_);
*/
    apr_status_t status;
    apr_thread_join(&status, th1);
    apr_thread_join(&status, th2);
/*
    apr_thread_join(&status, th3);
*/
    apr_time_t t2 = apr_time_now();
    double msg = MSG_COUNT * MSG_SIZE;
    int period = t2 - t1;
    double rate = (MSG_COUNT * 1000000.0) / period;
    printf("FINISHED SCTP CS TEST IN %d microsencds, RATE: %.2f MSG PER SECOND, THROUGHPUT: %.2fMB/s\n", period, rate, msg / period);
}

void test_udp_cs() {
    apr_thread_t *th1;
    apr_thread_t *th2;
    apr_thread_create(&th1, NULL, udp_cs_server, NULL, pl_);
    sleep(1);
    apr_time_t t1 = apr_time_now();
    apr_thread_create(&th2, NULL, udp_cs_client, NULL, pl_);
    apr_status_t status;
    apr_thread_join(&status, th1);
    apr_thread_join(&status, th2);
    apr_time_t t2 = apr_time_now();
    double msg = MSG_COUNT * MSG_SIZE;
    int period = t2 - t1;
    double rate = (MSG_COUNT * 1000000.0) / period;
    printf("FINISHED UDP CS TEST IN %d microsencds, RATE: %.2f MSG PER SECOND, THROUGHPUT: %.2fMB/s\n", period, rate, msg / period);
}

void test_udp_ss() {
    apr_thread_t *th1;
    apr_thread_t *th2;
    apr_thread_create(&th1, NULL, udp_cs_server, NULL, pl_);
    sleep(1);
    apr_time_t t1 = apr_time_now();
    apr_thread_create(&th2, NULL, udp_cs_client, NULL, pl_);
    apr_status_t status;
    apr_thread_join(&status, th1);
    apr_thread_join(&status, th2);
    apr_time_t t2 = apr_time_now();
    double msg = MSG_COUNT * MSG_SIZE;
    int period = t2 - t1;
    double rate = (MSG_COUNT * 1000000.0) / period;
    printf("FINISHED UDP CS TEST IN %d microsencds, RATE: %.2f MSG PER SECOND, THROUGHPUT: %.2fMB/s\n", period, rate, msg / period);
}

void test_apr_network() {
    apr_initialize();
    apr_pool_create(&pl_, NULL);
    
    //test_udp_cs();
    //test_sctp_cs();
    test_sctp_tcp_cs();
/*
    test_sctp_ss();
*/
    
    apr_terminate();
}

int main() {
    
    test_apr_network();
            
    return 0; 
}