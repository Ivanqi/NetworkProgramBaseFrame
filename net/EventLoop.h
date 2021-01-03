#ifndef EVENT_EVENTLOOP_H
#define EVENT_EVENTLOOP_H

#include <atomic>
#include <functional>
#include <vector>

#include <boost/any.hpp>

#include "MutexLock.h"
#include "CurrentThread.h"
#include "Timestamp.h"
#include "Callbacks.h"
#include "TimerId.h"

class Channel;
class Poller;
class TimerQueue;

// Reactor, 每个线程最多一个
// 接口类
class EventLoop
{
    public:
        typedef std::function<void()> Functor;
    
    private:
        typedef std::vector<Channel*> ChannelList;

        bool looping_;  // atomic

        std::atomic<bool> quit_;

        bool eventHandling_;    // atomic

        bool callingPendingFunctors_;   // atomic

        int64_t iteration_;

        const pid_t threadId_;

        Timestamp pollReturnTime_;

        std::unique_ptr<Poller> poller_;

        std::unique_ptr<TimerQueue> timerQueue_;

        int wakeupFd_;  // epollfd

        /**
         * 与TimerQueue不同，TimerQueue是一个内部类，我们不向客户机公开Channel
         */
        std::unique_ptr<Channel> wakeupChannel_;

        boost::any context_;

        // 暂存变量
        ChannelList activeChannels_;

        Channel* currentActiveChannel_;

        mutable MutexLock mutex_;

        std::vector<Functor> pendingFunctors_;
    
    public:
        EventLoop();

        ~EventLoop();

        /**
         * 永远循环
         * 必须在创建对象的同一线程中调用
         */
        void loop();

        /**
         * 退出循环
         * 如果这个线程不是100%安全的话,为了100%的安全性，最好通过共享的<EventLoop>调用
         */
        void quit();

        // 轮询返回的时间通常意味着数据到达
        Timestamp pollReturnTime() const
        {
            return pollReturnTime_;
        }

        int64_t iteration() const
        {
            return iteration_;
        }

        /**
         * 在循环线程中立即运行回调
         * 它唤醒循环，运行cb。如果在同一个循环线程中，cb在函数中运行
         * 从其他线程调用是安全的
         */
        void runInLoop(Functor cb);

        /**
         * 在循环线程中排队回调
         * 在完成池后运行,从其他线程调用是安全的
         */
        void queueInLoop(Functor cb);

        size_t queueSize() const;

        /**
         * 在“time”运行回调
         * 从其他线程调用是安全的
         */
        TimerId runAt(Timestamp time, TimerCallback cb);

        /**
         * 在 delay秒后运行回调
         * 从其他线程调用是安全的
         */
        TimerId runAfter(double delay, TimerCallback cb);

        /**
         * 每隔间隔秒运行一次回调
         * 从其他线程调用是安全的
         */
        TimerId runEvery(double interval, TimerCallback cb);

        /**
         * 取消计时器
         * 从其他线程调用是安全的
         */
        void cancel(TimerId timerId);

        void wakeup();
        void updateChannel(Channel *channel);
        void removeChannel(Channel *channel);
        bool hasChannel(Channel *channel);

        // 判断是不是同一个线程
        void assertInLoopThread()
        {
            if (!isInLoopThread()) {
                abortNotInLoopThread();
            }
        }

        // 判断当前线程是否变化
        bool isInLoopThread() const 
        {
            return threadId_ == CurrentThread::tid();
        }

        bool eventHandling() const
        {
            return eventHandling_;
        }

        void setContext(const boost::any& context)
        {
            context_ = context;
        }

        const boost::any& getContext() const
        {
            return context_;
        }

        boost::any* getMutableContext()
        {
            return &context_;
        }

        static EventLoop* getEventLoopOfCurrentThread();
    
    public:
        void abortNotInLoopThread();

        void handleRead();  // wake up

        void doPendingFunctors();

        void printActiveChannels() const;   //DEBUG
};

#endif