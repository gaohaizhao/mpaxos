
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libmpaxos/utils/mht.h"

int main() {
    mht_map_init("map1");
    mht_put("map1", "test data", 10, "test data", 10);  
    printf("test 1 %s\n", (char*)mht_get("map1", "test data", 10));
    mht_map_destroy("map1");

    char *k = malloc(6);
    k = strcpy(k, "hello");
    mht_put("map2", k, strlen(k) + 1, k, 6);
    free(k);
    printf("test 2 %s\n", (char*)mht_get("map2", "hello", 6));
}
