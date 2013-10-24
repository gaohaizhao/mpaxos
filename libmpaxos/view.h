#ifndef VIEW_H_
#define VIEW_H_

#include <stdbool.h>
#include "mpaxos/mpaxos-types.h"

typedef struct {
    nodeid_t nid;
    int port;
    char name[100];
    char ip[100];
} node_info_t;

void view_init();

void view_destroy();

void set_nodename(const char *nodename);

void set_node(const char* nodename, const char* addr, int port);

int get_group_size(groupid_t gid);

void set_local_nid(nodeid_t nid);

nodeid_t get_local_nid();

bool is_in_group(groupid_t gid);

int set_gid_nid(groupid_t gid, nodeid_t nid);

apr_array_header_t *get_group_nodes(groupid_t gid); 

apr_hash_t* view_group_table(groupid_t);

void get_all_groupids(groupid_t **gids, size_t *sz_gids);

#endif

