/*
 * test_mpaxos.c
 *
 *  Created on: Jan 29, 2013
 *      Author: ms
 */

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>
#include <json/json.h>
#include <apr_time.h>

#include "mpaxos/mpaxos.h"
#include "comm.h"
#include "utils/hostname.h"
#include "utils/logger.h"

int mpaxos_load_config(char *cf) {
    int local_nid;
    LOG_INFO("loading config file %s ...\n", cf);

    FILE* fp;
    if (cf == NULL) {
        fp = fopen("config.json", "r");
    } else {
        fp = fopen(cf, "r");
    }
    if (fp == NULL) {
        LOG_ERROR("Cannot open config file: %s.\n", cf);
        return 1;
    }
    size_t size = 10 * 1024 * 1024;
    char *buf = (char*) calloc(size, sizeof(char));
    fread(buf, 1, size, fp);
    fclose(fp);

    struct json_object *new_obj;
    new_obj = json_tokener_parse(buf);
    printf("%s\n", json_object_to_json_string(new_obj));

    struct json_object *val_obj;
    val_obj = json_object_object_get(new_obj, "local_nid");
    local_nid = json_object_get_int(val_obj);

    val_obj = json_object_object_get(new_obj, "local_port");
    int port = json_object_get_int(val_obj);
    set_listen_port(port);

    printf("local nid:%d\n", local_nid);
    printf("local port:%d\n", port);

    // set local nid
    set_local_nid(local_nid);

    struct json_object *nodes_array;
    nodes_array = json_object_object_get(new_obj, "nodes");
    int alen = json_object_array_length(nodes_array);
    int i;
    for (i = 0; i < alen; i++) {
        printf("node:\n");
        struct json_object *node_obj;
        node_obj = json_object_array_get_idx(nodes_array, i);
        struct json_object *val_obj;
        val_obj = json_object_object_get(node_obj, "nid");
        int nid = json_object_get_int(val_obj);
        //groups addr port
        struct json_object *node_groups_array;
        node_groups_array = json_object_object_get(node_obj, "groups");
        int glen = json_object_array_length(node_groups_array);
        int j;
        for (j = 0; j < glen; j++) {
            val_obj = json_object_array_get_idx(node_groups_array, j);
            groupid_t gid = json_object_get_int(val_obj);
            
            // set relation of gid and nid 
            set_gid_nid(gid, nid);
        }
        val_obj = json_object_object_get(node_obj, "addr");
        const char *addr = json_object_get_string(val_obj);
        val_obj = json_object_object_get(node_obj, "port");
        int port = json_object_get_int(val_obj);

        printf("\tnid:%d\n", nid);
        printf("\taddr:%s\n", addr);
        printf("\tport:%d\n", port);

        // set a sender
        set_nid_sender(nid, gethostip(addr), port);
    }

    // start server
    //start_server(local_port);
    json_object_put(new_obj);
    free(buf);
                       
    LOG_INFO("Config file loaded\n");
    return 0;
}

/* vim: set et ai ts=4 sw=4: */
