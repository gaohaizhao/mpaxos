#ifndef __LOCK_H__
#define __LOCK_H__

#include <pthread.h>
#include <assert.h>

struct ScopeLock {
    private:
        pthread_mutex_t *m_;
    public:
        ScopeLock(pthread_mutex_t *m): m_(m) {
            assert(pthread_mutex_lock(m_)==0);
        }
        ~ScopeLock() {
            assert(pthread_mutex_unlock(m_)==0);
        }
};

class Lock {
private:
    pthread_mutex_t mutex;
    pthread_cond_t cond;

public:
    Lock() : mutex(PTHREAD_MUTEX_INITIALIZER), cond(PTHREAD_COND_INITIALIZER) {
    }

    void lock() {
        pthread_mutex_lock(&mutex);
    }

    void wait() {
        pthread_cond_wait(&cond, &mutex);
    }

    void unlock() {
        pthread_mutex_unlock(&mutex);
    }

    void signal() {
        pthread_cond_signal(&cond);
    }
};


#endif
