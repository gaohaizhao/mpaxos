#include <stdio.h>
#include <stdlib.h>

#include <apr_hash.h>


int main(int argc, char** argv) {
    apr_pool_t *mp;
    apr_hash_t *ht;

    //Initialize
    apr_initialize();
    apr_pool_create(&mp, NULL);

    ht = apr_hash_make(mp);

//  char key[100] = "key1";
//  char val[100] = "val1";
//  char val2[100] = "val2";
//
//  //test a string
//  //apr_hash_set(ht, key, APR_HASH_KEY_STRING, val);
//  void *val_ptr;
//  //val_ptr = apr_hash_get(ht, key, APR_HASH_KEY_STRING);
//  //printf("%s\n", val_ptr);
//
//  //test for different key;
//  char *k1 = (char *)malloc(100);
//  char *k2 = (char *)malloc(100);
//  strcpy(k1, key);
//  strcpy(k2, key);
//  apr_hash_set(ht, k1, APR_HASH_KEY_STRING, val);
////    free(k1);
//  val_ptr = apr_hash_get(ht, k2, APR_HASH_KEY_STRING);
////    printf("%s\t", 
//  printf("%s\n", val_ptr);
//
////    free(k1);
//
//  printf("count:%d\n", apr_hash_count(ht));
//
//  apr_hash_set(ht, k2, APR_HASH_KEY_STRING, val);
//  free(k1);
//  free(k2);
//  
//  apr_hash_index_t *hi;
//  for (hi = apr_hash_first(mp, ht); hi; hi = apr_hash_next(hi)) {
//      void *k, *v;
//      apr_hash_this(hi, &k, NULL, &v);
//      printf("k:%s\t", k);
//      printf("v:%s\n", v);
//  }

//  // TEST FOR INVALID KEY;
//  // This test has some problem.
//  int COUNT = 10000000;
//  char *key;
//  char *value;
//  int i;
//  for (i = 0; i < COUNT; i++) {
//      key = (char *)malloc(10);
//      memset(key, 0, 10);
//      value   = (char *)malloc(10);
//      sprintf(key, "%d", i);
//      sprintf(value, "%d", i);
//      apr_hash_set(ht, key, 10, value);
//      free(key);  
//  }
//
//  printf("Start to iterate.\n");
//  apr_hash_index_t *hi;
//  for (hi = apr_hash_first(mp, ht); hi; hi = apr_hash_next(hi)) {
//      void *k, *v;
//      apr_hash_this(hi, &k, NULL, &v);
//  //  printf("k:%s\t", k);
//      printf("v:%s\n", v);
//  }
//
//  printf("Start to validate.\n");
//  for (i = 0; i < COUNT; i++) {
//      char *k = (char*) malloc(10);
//      memset(k, 0, 10);
//      char *v;
//      sprintf(k, "%d", i);
//      v = apr_hash_get(ht, k, 10);
//      if (v == NULL) {
//          printf("ERROR, value is null");
//          break;
//      }
//      int vint = atoi(v);
//      printf("Test key:%s value:%s\n", k, v);
//      if (vint != i) {
//          printf("ERROR, not equal %d %d", i, vint);
//      }
//      free(k);
//  }

//  char key[100];
//  char val[100];
//  sprintf(value, "hello world!");
//  sprintf(key, "k1");
//  apr_hash_set(ht, key, 2, val);
////    sprintf(key, "k2");
//  void *r = apr_hash_get(ht, "k1", 2);
//  if (r == NULL) {
//      printf("cannot find k1\n");
//  } else {
//      printf("found k1, val: %s\n", r);
//  }
//
//  r = apr_hash_get(ht, "k2", 2);
//  if (r == NULL) {
//      printf("cannot find k2\n");
//  } else {
//      printf("found k2, val: %s\n", r);
//  }

    
    char k1[100];
    char k2[100];
    char val[100];
    sprintf(val, "hello world!");
    sprintf(k1, "key");
    sprintf(k2, "key");
    apr_hash_set(ht, k1, 3, val);
    apr_hash_set(ht, k2, 3, val);

    sprintf(k1, "key1");
    sprintf(k2, "key2");
    
    apr_hash_index_t *hi;
    for (hi = apr_hash_first(mp, ht); hi; hi = apr_hash_next(hi)) {
        void *k, *v;
        apr_hash_this(hi, &k, NULL, &v);
        printf("k:%s\t", k);
        printf("v:%s\n", v);
    }

    //Finalize;
    apr_pool_destroy(mp);
    apr_terminate();
    return 0;
}
