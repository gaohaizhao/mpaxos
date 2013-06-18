#include <stdio.h>
#include <json/json.h>

int main() {
    FILE* fp;
    fp = fopen("config.json", "r");
    if (fp == NULL) {
        printf("File not exist.\n");
        exit(0);
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
    int local_nid = json_object_get_int(val_obj);

    printf("local nid:%d\n", local_nid);

    struct json_object *nodes_array;
    nodes_array = json_object_object_get(new_obj, "nodes");
    int alen = json_object_array_length(nodes_array);
    int i;
    for (i = 0; i < alen; i++) {
        printf("node:\n");
        struct json_object *node_object;
        node_object = json_object_array_get_idx(nodes_array, i);
        struct json_object *val_obj;
        val_obj = json_object_object_get(node_object, "nid");
        int nid = json_object_get_int(val_obj);
        printf("\tnid:%d\n", nid);
    }
    // json_object_put(nodes_array);
    // json_object_put(val_obj);
    json_object_put(new_obj);
    free(buf);
}
