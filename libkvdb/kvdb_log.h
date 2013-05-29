#ifndef __ZDEBUG_H__
#define __ZDEBUG_H__

#ifdef __cplusplus
#include <cstdio>
#else
#include <stdio.h>
#endif
#include <sys/time.h>

#define DEBUG

#ifdef DEBUG
    #define loglevel 0

    #define PUT(t, x, ...) printf("[%s:%d] [ %s ] " x "\n", __FILE__, __LINE__, t , ##__VA_ARGS__)
    #define EE(x...) if (loglevel <= 6) PUT("EE", x)
    #define II(x...) if (loglevel <= 4) PUT("II", x)
    #define WW(x...) if (loglevel <= 5) PUT("WW", x)
    #define VV(x...) if (loglevel <= 2) PUT("VV", x)
    #define DD(x...) if (loglevel <= 3) PUT("DD", x)
    #define FF printf(" [ FF ] : %s %d\n", __FILE__, __LINE__)
#else
    #define PUT(t, x, ...)
    #define EE(x...)
    #define II(x...)
    #define WW(x...)
    #define VV(x...)
    #define DD(x...)
    #define FF
#endif

static struct timeval profile_begin;
static struct timeval profile_end;

static double timeval_double(struct timeval t) {
    return t.tv_sec + t.tv_usec / 1000000.0;
}

static double timer_begin() {
    gettimeofday(&profile_begin, NULL);
    return timeval_double(profile_begin);
}

static double timer_end() {
    gettimeofday(&profile_end, NULL);
    return timeval_double(profile_end);
}

static double timer_duration() {
    return timeval_double(profile_end) - timeval_double(profile_begin);
}

static void timer_print() {
    printf("%f\n", timer_duration());
}

#define ELAPSED(x) timer_begin(); printf(#x": "); x; timer_end(); timer_print()

#endif  // __ZDEBUG_H__

