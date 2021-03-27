#ifndef EVENT_COUNTDOWNLATCH_H
#define EVENT_COUNTDOWNLATCH_H


#include <boost/noncopyable.hpp>
#include "Condition.h"
#include "MutexLock.h"

/**
 * CountDownLatch的主要作用是确保Thread中传进去的func真的启动了以后
 * 外层的start才返回
 */
class CountDownLatch: boost::noncopyable
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

#endif