

#include <apr_hash.h>


int is_init = 0;

void mht_init() {

    is_init = 1;
}

void mht_map_init(uint32_t id) {
    if (!is_init) {
        mht_init();
    }
}

void mht_map_destroy(uint32_t id) {

}

void mht_put(uint32_t id, void* k, size_t sz_k, void* v, size_t sz_v) {

}

void mht_get(uint32_t id, void* k, size_t sz_k, void** v) {

}

void mht_destroy(uint32_t id) {

}
