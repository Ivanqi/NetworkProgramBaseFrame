#ifndef NETWORKER_BASE_CONDITION_H
#define NETWORKER_BASE_CONDITION_H

#include <pthread.h>
#include <errno.h>
#include <cstdint>
#include <time.h>
#include "MutexLock.h"

namespace networker
{
    class Condition : noncopyable 
    {
        private:
            MutexLock &mutex;
            pthread_cond_t cond;

        public:
            explicit Condition(MutexLock &_mutex)
                :mutex(_mutex)
            {
                pthread_cond_init(&cond, NULL);
            }

            ~Condition() 
            {
                pthread_cond_destroy(&cond);
            }

            void wait() 
            {
                pthread_cond_wait(&cond, mutex.get());
            }

            void notify() 
            {
                pthread_cond_signal(&cond);
            }

            void notifyAll() 
            {
                pthread_cond_broadcast(&cond);
            }

            bool waitForSeconds(int seconds) 
            {
                struct timespec abstime;
                clock_gettime(CLOCK_REALTIME, &abstime);
                abstime.tv_sec += static_cast<time_t>(seconds);
                return ETIMEDOUT == pthread_cond_timedwait(&cond, mutex.get(), &abstime);
            }
    };
};


#endif