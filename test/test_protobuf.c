
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <apr_time.h>
#include "mpaxos.pb-c.h"

int main(int argc, char **argv) {
    Mpaxos__MsgPrepare prep = MPAXOS__MSG_PREPARE__INIT;
    Mpaxos__MsgAccept msg_accp = MPAXOS__MSG_ACCEPT__INIT;
    Mpaxos__MsgHeader header = MPAXOS__MSG_HEADER__INIT;
    Mpaxos__ProcessidT pid = MPAXOS__PROCESSID_T__INIT;

    prep.h = &header;
    prep.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__PREPARE;
    prep.h->pid = &pid;
    prep.h->pid->gid = 1;
    prep.h->pid->nid = 1;

    Mpaxos__Proposal prop = MPAXOS__PROPOSAL__INIT;
    char data_char[100] = "helloworld\0";

    msg_accp.h = &header;
    msg_accp.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPT;
    msg_accp.h->pid = &pid;
    msg_accp.h->pid->gid = 1;
    msg_accp.h->pid->nid = 1;
    msg_accp.prop = &prop;
    msg_accp.prop->value.data = data_char;
    msg_accp.prop->value.len = 12;

    int len;
    char buf[1000];

    len = mpaxos__msg_accept__get_packed_size(&msg_accp);
    mpaxos__msg_accept__pack(&msg_accp, buf);

    FILE *fp;
    fp = fopen("test.out", "w");
    fwrite(buf, 1, len, fp);
    fclose(fp);

    memset(buf, 0, sizeof(buf));
    fp = fopen("test.out", "r");
    int n = fread(buf, 1, 1024, fp);
    fclose(fp);


    Mpaxos__MsgCommon *msg_comm_ptr;
    Mpaxos__MsgAccept *msg_accp_ptr;
    msg_comm_ptr = mpaxos__msg_common__unpack(NULL, n, buf);
    msg_accp_ptr = mpaxos__msg_accept__unpack(NULL, n, buf);


    printf("%d\n", msg_comm_ptr->h->pid->gid);
    printf("%d\n", msg_accp_ptr->h->pid->nid);
    printf("%s\n", msg_accp_ptr->prop->value.data);

    mpaxos__msg_common__free_unpacked(msg_comm_ptr, NULL);
    mpaxos__msg_accept__free_unpacked(msg_accp_ptr, NULL);

    apr_time_t t1 = apr_time_now();
    printf("TEST FOR PERFORMANCE.\n");
    for (int i = 0; i < 10000; i++) {
        Mpaxos__MsgAccept msg_accp = MPAXOS__MSG_ACCEPT__INIT;
        Mpaxos__MsgHeader header = MPAXOS__MSG_HEADER__INIT;
        Mpaxos__ProcessidT pid = MPAXOS__PROCESSID_T__INIT;

        prep.h = &header;
        prep.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__PREPARE;
        prep.h->pid = &pid;
        prep.h->pid->gid = 1;
        prep.h->pid->nid = 1;

        Mpaxos__Proposal prop = MPAXOS__PROPOSAL__INIT;
        char *data = (char*)malloc(1024);

        msg_accp.h = &header;
        msg_accp.h->t = MPAXOS__MSG_HEADER__MSGTYPE_T__ACCEPT;
        msg_accp.h->pid = &pid;
        msg_accp.h->pid->gid = 1;
        msg_accp.h->pid->nid = 1;
        msg_accp.prop = &prop;
        msg_accp.prop->value.data = data;
        msg_accp.prop->value.len = 1024;
        len = mpaxos__msg_accept__get_packed_size(&msg_accp);
        char *buf = malloc(len);
        mpaxos__msg_accept__pack(&msg_accp, buf);
        Mpaxos__MsgAccept *msg_accp_ptr;
        msg_accp_ptr = mpaxos__msg_accept__unpack(NULL, len, buf);
        mpaxos__msg_accept__free_unpacked(msg_accp_ptr, NULL);
        free(buf);
        free(data);
    }
    apr_time_t t2 = apr_time_now();
    printf("10000 accept msg cost %lld us, rate:%lld per sec..\n", (t2-t1), 10000000000/(t2-t1));
    
    return 0;
}

