#ifndef EVENT_EVENTLOOPTHREAD_H
#define EVENT_EVENTLOOPTHREAD_H

#include "Condition.h"
#include "MutexLock.h"
#include "Thread.h"


namespace networker
{
namespace net
{
    class EventLoop;

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