
#include "utils/mpr_hash.h"


int simple_fun__ = 0;

void simple_fun() {
    printf("simple function called.\n");
    simple_fun__++;
}

START_TEST(mpr_hash) {
    mpr_hash_t *ht;
    uint8_t k1 = 5; 
    uint16_t k2 = 6; 
    uint32_t k3 = 7; 
    uint64_t k4 = 8; 
    int t = 10;

    mpr_hash_create(&ht);
    mpr_hash_set(ht, &k1, sizeof(uint8_t), &t, sizeof(int)) ;
    mpr_hash_set(ht, &k2, sizeof(uint16_t), &t, sizeof(int)); 
    mpr_hash_set(ht, &k3, sizeof(uint32_t), &t, sizeof(int)); 
    mpr_hash_set(ht, &k4, sizeof(uint64_t), &t, sizeof(int)); 

    int *res = NULL;
    size_t sz;
    uint8_t kk1 = 5; 
    uint16_t kk2 = 6; 
    uint32_t kk3 = 7; 
    uint64_t kk4 = 8; 
    mpr_hash_get(ht, &kk1, sizeof(uint8_t), (void**)&res, &sz);
    ck_assert(res != NULL && *res == 10);
    mpr_hash_get(ht, &kk2, sizeof(uint16_t), (void**)&res, &sz);
    ck_assert(res != NULL && *res == 10);
    mpr_hash_get(ht, &kk3, sizeof(uint32_t), (void**)&res, &sz);
    ck_assert(res != NULL && *res == 10);
    mpr_hash_get(ht, &kk4, sizeof(uint64_t), (void**)&res, &sz);
    ck_assert(res != NULL && *res == 10);

    uint8_t kfun = 9;
    void *sfun = simple_fun;
    mpr_hash_set(ht, &kfun, sizeof(uint8_t), &sfun, sizeof(void *));
    void (**fun)() = NULL;
    mpr_hash_get(ht, &kfun, sizeof(uint8_t), (void**)&fun, &sz);
    printf("fun %x\n", simple_fun);
    printf("fun %x\n", *fun);
    ck_assert(fun != NULL); 
    (**fun)();
    ck_assert(simple_fun__ == 1);
      
    mpr_hash_destroy(ht);
}
END_TEST
