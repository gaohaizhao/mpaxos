gcc test_bdb.c -ldb -g -pg `pkg-config --cflags --libs apr-1` -std=c99 -o ../bin/test_bdb.out
