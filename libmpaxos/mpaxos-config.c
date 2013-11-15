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
#include "view.h"
#include "utils/hostname.h"
#include "utils/logger.h"

int mpaxos_config_load(const char *cf) {
    LOG_INFO("loading config file %s ...\n", cf);

    FILE* fp;
    if (cf == NULL) {
        fp = fopen("config.json", "r");
    } else {
        fp = fopen(cf, "r");
    }
    if (fp == NULL) {
        LOG_ERROR("cannot open config file: %s.\n", cf);
        return -1;
    }
    size_t size = 10 * 1024 * 1024; //10M buffer, i think that is enough
    char *buf = (char*) calloc(size, sizeof(char));
    fread(buf, 1, size, fp);
    fclose(fp);

    struct json_object *new_obj;
    new_obj = json_tokener_parse(buf);
    //printf("%s\n", json_object_to_json_string(new_obj));

    struct json_object *nodes_array;
    nodes_array = json_object_object_get(new_obj, "nodes");
    SAFE_ASSERT(nodes_array != NULL);

    int alen = json_object_array_length(nodes_array);
    for (int i = 0; i < alen; i++) {
        struct json_object *node_obj;
        node_obj = json_object_array_get_idx(nodes_array, i);
        struct json_object *val_obj;

        val_obj = json_object_object_get(node_obj, "nid");
        int nid = json_object_get_int(val_obj);
        //groups addr port
        struct json_object *node_groups_array;

        // currently not groups. dynamically set in API.
//        node_groups_array = json_object_object_get(node_obj, "groups");
//        int glen = json_object_array_length(node_groups_array);
//        int j;
//        for (j = 0; j < glen; j++) {
//            val_obj = json_object_array_get_idx(node_groups_array, j);
//            groupid_t gid = json_object_get_int(val_obj);
//            
//            // set relation of gid and nid 
//            set_gid_nid(gid, nid);
//        }
//
        val_obj = json_object_object_get(node_obj, "name");
        const char *name = json_object_get_string(val_obj);
        
        val_obj = json_object_object_get(node_obj, "addr");
        const char *addr = json_object_get_string(val_obj);

        val_obj = json_object_object_get(node_obj, "port");
        int port = json_object_get_int(val_obj);

        // set a node in view
        set_node(name, addr, port);
    }
    
    struct json_object *val_obj;

    val_obj = json_object_object_get(new_obj, "nodename");
    if (val_obj != NULL) {
        const char* nodename = json_object_get_string(val_obj);
        mpaxos_config_set("nodename", nodename); 
    }

    json_object_put(new_obj);
    free(buf);
                       
    LOG_INFO("config file loaded\n");
    return 0;
}

int mpaxos_config_set(const char *key, const char *value) {
    if (strcmp(key, "nodename") == 0) {
        set_nodename(value);                 
    }
    return 0;
}

int mpaxos_config_get(const char *key, char** value) {
    return 0;
}

