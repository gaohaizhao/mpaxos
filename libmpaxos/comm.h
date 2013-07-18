#ifndef COMM_H_
#define COMM_H_

#include <stdint.h>
#include <stdbool.h>
#include "sendrecv.h"
#include "mpaxos/mpaxos.h"
#include "mpaxos/mpaxos-types.h"


void comm_init();
void comm_final();

void set_local_nid(nodeid_t nid);
nodeid_t get_local_nid();

bool is_in_group(groupid_t gid);
int set_gid_nid(groupid_t gid, nodeid_t nid);
void set_nid_sender(nodeid_t nid, const char* addr, int port);


void add_group(groupid_t);
void add_group_sender(groupid_t, nodeid_t);

void add_sender(uint32_t nid, const char *remote_ip,
      uint16_t remote_port);

void add_recvr(void* on_recv);

void send_to(nodeid_t nid, const uint8_t *, size_t sz);
void send_to_group(groupid_t gid, const uint8_t *buf, size_t sz);
void send_to_groups(groupid_t* gids, uint32_t gids_len,
          const char *buf, size_t sz);
//void send_to_all(const char *buf, size_t sz);
void start_server(int port);

void connect_all_senders();

void* APR_THREAD_FUNC on_recv(apr_thread_t *th, void* arg);

slotid_t send_to_slot_mgr(groupid_t, nodeid_t, uint8_t *, size_t);

#endif
