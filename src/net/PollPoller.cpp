#include "PollPoller.h"

#include "Types.h"
#include "Channel.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>

PollPoller::PollPoller(EventLoop *loop): Poller(loop)
{

}

PollPoller::~PollPoller() = default;

Timestamp PollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // XXX pollfds_ shouldn't change
    int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
    int saveErrno = errno;

    Timestamp now(Timestamp::now());

    if (numEvents > 0) {    // poll 事件触发成功
        printf("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
    } else if (numEvents == 0) {
        printf(" nothing happened");
    } else {
        if (saveErrno != EINTR) {
            errno = saveErrno;
            printf("PollPoller::poll()");
        }
    }
    return now;
}

void PollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    // 遍历所有的文件描述符，O(N)
    // 为了提前结束循环，每找到一个活动fd就递减numEvents，这样当numEvents减为0时表示活动fd都找完了
    for (PollFdList::const_iterator pfd = pollfds_.begin(); pfd != pollfds_.end() && numEvents > 0; ++pfd) {
        // pfd->revents > 0，一般包含, POLLIN, POLLPRI, POLLOUT, POLLRDHUP, POLLERR, POLLHUP
        if (pfd->revents > 0) {
            --numEvents;
            ChannelMap::const_iterator ch = channels_.find(pfd->fd);
            assert(ch != channels_.end());

            // 获取fd所对应的channel
            Channel *channel = ch->second;
            // 相互验证
            assert(channel->fd() == pfd->fd);

            // 把所得的channel加入到EventLoop 的activeChannels中
            activeChannels->push_back(channel);
        }
    }
}

// 新增或修改poll 事件。添加新Channel O(logN)，更新已有的ChannelO(1)
void PollPoller::updateChannel(Channel *channel)
{
    Poller::assertInLoopThread();
    printf("fd = %d events = %d\n", channel->fd(), channel->events());

    if (channel->index() < 0) {
        // 一个新的，添加到pollfds_
        assert(channels_.find(channel->fd()) == channels_.end());

        struct pollfd pfd;

        // 设置多个pollfd的fd, events和revents
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        pollfds_.push_back(pfd);

        // 更新channel的index值, index = pollfds_.size() - 1
        int idx = static_cast<int>(pollfds_.size()) - 1;
        channel->set_index(idx);
        
        // 维护pfd.fd 和 channel 的映射关系 
        channels_[pfd.fd] = channel;

    } else {
        // 更新现有的
        assert(channels_.find(channel->fd()) != channels_.end());
        assert(channels_[channel->fd()] == channel);

        int idx = channel->index();
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

        struct pollfd& pfd = pollfds_[idx];
        assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1);

        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;

        if (channel->isNoneEvent()) {
            // 如果某个Channel暂时不关心任何事件，就把pollfd.fd设置为，-channel->fd() - 1 然后poll忽略此项
            pfd.fd = -channel->fd() - 1;
        }
    }
}

void PollPoller::removeChannel(Channel *channel)
{
    Poller::assertInLoopThread();
    printf("fd = %d\n", channel->fd());

    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    assert(channel->isNoneEvent());

    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

    const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
    assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());

    size_t n = channels_.erase(channel->fd());
    assert(n == 1); (void)n;

    if (implicit_cast<size_t>(idx) == pollfds_.size() - 1) {
        pollfds_.pop_back();
    } else {
        int channelAtEnd = pollfds_.back().fd;
        iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
        if (channelAtEnd < 0) {
            channelAtEnd = -channelAtEnd - 1;
        }

        channels_[channelAtEnd]->set_index(idx);
        pollfds_.pop_back();
    }
}