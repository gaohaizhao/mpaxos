#ifndef __SCOPE_LOCK__
#define __SCOPE_LOCK__

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
#endif  /*__SCOPE_LOCK__*/
