#ifndef NETWORKER_NET_EVENTLOOPTHREAD_H
#define NETWORKER_NET_EVENTLOOPTHREAD_H

#include "networker/base/Condition.h"
#include "networker/base/MutexLock.h"
#include "networker/base/Thread.h"


namespace networker
{
namespace net
{
    class EventLoop;

    /**
     * IO线程不一定是主线程，可以在任何一个线程创建并运行EventLoop
     * 一个程序也可以有不止一个IO线程，可以按优先级将不同的socket分给不同的IO线程，避免优先级反转
     */
    class EventLoopThread: noncopyable
    {
        public:
            typedef std::function<void(EventLoop*)> ThreadInitCallback;

        private:
            EventLoop *loop_;
            bool exiting_;
            Thread thread_;
            MutexLock mutex_;

            Condition cond_;
            ThreadInitCallback callback_;
        
        public:
            EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const string& name = string());

            ~EventLoopThread();

            EventLoop *startLoop();
        
        private:
            void threadFunc();
    };
};
};



#endif