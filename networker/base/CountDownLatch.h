#ifndef NETWORKER_BASE_COUNTDOWNLATCH_H
#define NETWORKER_BASE_COUNTDOWNLATCH_H


#include "networker/base/Condition.h"
#include "networker/base/MutexLock.h"

namespace networker
{
    /**
     * CountDownLatch的主要作用是确保Thread中传进去的func真的启动了以后
     * 外层的start才返回
     */
    class CountDownLatch: noncopyable
    {
        private:
            mutable MutexLock mutex_;
            Condition condition_;
            int count_;

        public:
            explicit CountDownLatch(int count);

            void wait();

            void countDown();
    };
};
#endif