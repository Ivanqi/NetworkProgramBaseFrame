#ifndef EVENT_POLLPOLLER_H
#define EVENT_POLLPOLLER_H
#include "Poller.h"

#include <vector>

struct pollfd;

// IO Multiplexing with poll(2)
class PollPoller: public Poller
{
    private:
        typedef std::vector<struct pollfd> PollFdList;
        PollFdList pollfds_;    // 存储 poll的事件

    public:
        PollPoller(EventLoop *loop);

        ~PollPoller() override;

        Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;

        void updateChannel(Channel *channel) override;

        void removeChannel(Channel *channel) override;
    
    private:
        void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;

        
};

#endif