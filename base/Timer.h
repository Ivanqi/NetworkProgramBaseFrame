#ifndef EVENT_TIMER_H
#define EVENT_TIMER_H

#include "Atomic.h"
#include "Timestamp.h"
#include "Callbacks.h"

/**
 * 为了区分地址相同的先后两个Timer对象。因此每个Timer对象有一个全局递增的序列号int64_t sequence_
 */
class Timer
{
    private:
        const TimerCallback callback_;      // 回调函数
        Timestamp expiration_;              // 有效时间
        const double interval_;             // 间隔时间
        const bool repeat_;                 // interval > 0.0， 是否存在定时任务
        const int64_t sequence_;            // 获取序列号
        static AtomicInt64 s_numCreated_;   // 原子操作
    
    public:
        Timer(TimerCallback cb, Timestamp when, double interval)
            : callback_(std::move(cb)), expiration_(when),
            interval_(interval), repeat_(interval > 0.0),
            sequence_(s_numCreated_.incrementAndGet())
        {}

        void run() const
        {
            callback_();
        }

        Timestamp expiration() const
        {
            return expiration_;
        }

        bool repeat() const
        {
            return sequence_;
        }

        int64_t sequence() const
        {
            return sequence_;
        }

        void restart(Timestamp now);

        static int64_t numCreated()
        {
            return s_numCreated_.get();
        }
};

#endif