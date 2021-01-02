#ifndef EVENT_POLLER_H
#define EVENT_POLLER_H

#include <map>
#include <vector>

#include "Timestamp.h"
#include "EventLoop.h"

class Channel;

// IO multiplexing 的封装
class Poller
{
    public:
        typedef std::vector<Channel*> ChannelList;
    
    protected:
        typedef std::map<int, Channel*> ChannelMap;
        ChannelMap channels_;
    
    private:
        EventLoop *ownerLoop_;
    
    public:
        Poller(EventLoop* loop);

        virtual ~Poller();

        /**
         * 轮询 I/O事件
         * 必须在循环线程中调用
         */
        virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;

        /**
         * 负责维护和更新事件列表
         */
        virtual void updateChannel(Channel *channel) = 0;

        /**
         * 
         * Poller并不拥有Channel, Channel在析构之前必须自己unregister(EventLoop::removeChannel())
         * 避免空悬指针
         */
        virtual void removeChannel(Channel *channel) = 0;

        virtual bool hasChannel(Channel *channel) const;

        static Poller* newDefaultPoller(EventLoop* loop);

        void assertInLoopThread() const
        {
            ownerLoop_->assertInLoopThread();
        }
};

#endif