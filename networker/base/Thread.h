#ifndef NETWORKER_BASE_THREAD_H
#define NETWORKER_BASE_THREAD_H


#include <functional>
#include <memory>
#include <pthread.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "networker/base/Atomic.h"
#include "networker/base/CountDownLatch.h"
#include "networker/base/Types.h"

namespace networker
{
    class Thread: noncopyable
    {
        public:
            typedef std::function<void()> ThreadFunc;

        private:
            bool started_;
            bool joined_;
            pthread_t pthreadId_;
            pid_t tid_;
            ThreadFunc func_;
            string name_;
            CountDownLatch latch_;
            static AtomicInt32 numCreated_;

        public:

            explicit Thread (const ThreadFunc&, const string& name = string());

            ~Thread();
            
            void start();

            int join();

            bool started() const {
                return started_;
            };

            pid_t tid() const
            {
                return tid_;
            }

            const string& name() const
            {
                return name_;
            }

            static int numCreated()
            {
                return numCreated_.get();
            }

        private:
            void setDefaultName();
    };
};
#endif