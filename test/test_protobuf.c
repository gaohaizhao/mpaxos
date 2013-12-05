
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <apr_time.h>
#include "mpaxos.pb-c.h"

#define N_PROTO 100000

void pack_unpack_accept() {
    Mpaxos__MsgAccept msg_accp = MPAXOS__MSG_ACCEPT__INIT;
    Mpaxos__MsgHeader header = MPAXOS__MSG_HEADER__INIT;

//    Mpaxos__MsgPrepare msg_prep = MPAXOS__MSG_PREPARE__INIT;
//    msg_prep.h = &header;
//    msg_prep.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__PREPARE;

    Mpaxos__Proposal prop = MPAXOS__PROPOSAL__INIT;

    uint8_t* data = malloc(100);
    msg_accp.h = &header;
    msg_accp.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPT;
    msg_accp.prop = &prop;
    msg_accp.prop->value.data = data;
    msg_accp.prop->value.len = 100;

    // packing
    int len = mpaxos__msg_accept__get_packed_size(&msg_accp);
    uint8_t *buf = malloc(len);
    mpaxos__msg_accept__pack(&msg_accp, buf);

    // unpacking
    Mpaxos__MsgAccept *msg_accp_ptr;
    msg_accp_ptr = mpaxos__msg_accept__unpack(NULL, len, buf);
    mpaxos__msg_accept__free_unpacked(msg_accp_ptr, NULL);

    free(data);
    free(buf);
}

START_TEST(protobuf) {
    //Mpaxos__MsgCommon *msg_comm_ptr;
    //Mpaxos__MsgAccept *msg_accp_ptr;
    //msg_comm_ptr = mpaxos__msg_common__unpack(NULL, n, buf);
    //msg_accp_ptr = mpaxos__msg_accept__unpack(NULL, n, buf);


    //printf("%s\n", msg_accp_ptr->prop->value.data);

    //mpaxos__msg_common__free_unpacked(msg_comm_ptr, NULL);
    //mpaxos__msg_accept__free_unpacked(msg_accp_ptr, NULL);

    apr_time_t t1 = apr_time_now();
    printf("test for performance of protobuf-c.\n");
    for (int i = 0; i < N_PROTO; i++) {
        pack_unpack_accept();
    }
    apr_time_t t2 = apr_time_now();
    uint32_t period = (t2 - t1) / 1000;
    printf("packing and unpacking %d accept messages cost %dms, rate:%d per sec..\n", N_PROTO, period, N_PROTO * 1000 / period);
} END_TEST

