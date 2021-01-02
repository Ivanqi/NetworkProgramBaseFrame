#ifndef EVENT_TIMERQUEUE_H
#define EVENT_TIMERQUEUE_H

#include <set>
#include <vector>

#include "MutexLock.h"
#include "Timestamp.h"
#include "Callbacks.h"
#include "Channel.h"

class EventLoop;
class Timer;
class TimerId;

/**
 * 最大努力计时器队列
 * 不能保证回调会准时
 * 
 * 采用平衡二叉树来管理未到期的timers,因此这些操作的时间复杂度O(logN)
 * 
 * 在非阻塞服务端编程中，绝对不能用sleep()或类似的办法让程序原地停留等待，这回让程序失去响应
 * 因为主事件循环被挂起了，无法处理IO事件
 * 
 * 对于定时任务，把它变成一个特定的消息，到时候触发相应的消息处理函数
 * 
 * TimerQueue的成员函数只能在其所属的IO线程调用，因此不必加锁
 */
class TimerQueue
{
    private:
        // pair<Timestamp, Timer*>为key，这样即便两个Timer的到期时间相同，它们的地址也必定不同
        typedef std::pair<Timestamp, Timer*> Entry;
        typedef std::set<Entry> TimerList;
        typedef std::pair<Timer*, int64_t> ActiveTimer;
        typedef std::set<ActiveTimer> ActiveTimerSet;

        EventLoop* loop_;

        const int timerfd_;

        // 使用Channel来观察timerfd_上的readable时间
        Channel timerfdChannel_;

        // 按过期排序的计时器列表
        TimerList timers_;

        // 取消
        // 保存的是目前有效的Timer指针，并满足invariant: timers_.size() == activeTimers_.size()，因为这两个容器保存的是相同的数据
        // 只不过timers_是按到期事件排序，activeTimers_是按对象地址排序
        ActiveTimerSet activeTimers_;
        bool callingExpiredTimers_;     /* atomic */
        ActiveTimerSet cancelingTimers_;

    public:
        explicit TimerQueue(EventLoop *loop);
        ~TimerQueue();

        /**
         * 计划在给定时间运行回调，如果 interval>0.0，则重复
         * 线程安全的
         */
        TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

        void cancel(TimerId TimerId);
    
    private:
        void addTimerInLoop(Timer* timer);
        
        void cancelInLoop(TimerId timerId);

        // 当timerfd报警时调用
        void handleRead();

        // 移走所有过期的计时器
        std::vector<Entry> getExpired(Timestamp now);

        void reset(const std::vector<Entry>& expired, Timestamp now);

        bool insert(Timer* timer);
};

#endif