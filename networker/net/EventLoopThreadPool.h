#ifndef NETWORKER_NET_EVENTLOOPTHREADPOOL_H
#define NETWORKER_NET_EVENTLOOPTHREADPOOL_H

#include "networker/base/Types.h"
#include "networker/base/noncopyable.h"

#include <functional>
#include <memory>
#include <vector>

namespace networker
{
namespace net
{
    class EventLoop;
    class EventLoopThread;

    class EventLoopThreadPool: noncopyable
    {
        private:
            EventLoop *baseLoop_;
            string name_;
            bool started_;
            int numThreads_;
            int next_;
            std::vector<std::unique_ptr<EventLoopThread>> threads_;
            std::vector<EventLoop*> loops_;
        
        public:
            typedef std::function<void(EventLoop*)> ThreadInitCallback;
            
            EventLoopThreadPool(EventLoop *baseLoop, const string& nameArg);

            ~EventLoopThreadPool();

            void setThreadNum(int numThreads)
            {
                numThreads_ = numThreads;
            }

            void start(const ThreadInitCallback& cb = ThreadInitCallback());

            // 调用start才生效
            // 使用 round-robin 算法
            EventLoop *getNextLoop();

            // 使用相同的哈希代码，它将始终返回相同的EventLoop
            EventLoop *getLoopForHash(size_t hashCode);

            std::vector<EventLoop*> getAllLoops();

            bool started() const
            {
                return started_;
            }

            const string& name() const
            {
                return name_;
            }
    };
};
};

#endif