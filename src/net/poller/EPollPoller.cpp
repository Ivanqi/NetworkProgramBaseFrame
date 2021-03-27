#include "EPollPoller.h"
#include "Channel.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <unistd.h>

// 在Linux上，poll（2）和epoll（4）的常量应该相同
static_assert(EPOLLIN == POLLIN,        "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI,      "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT,      "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP,  "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR,      "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP,      "epoll uses same flag values as poll");

const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop): Poller(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollfd_ < 0) {
        printf("EPollPoller::EPollPoller error\n");
    }
}


EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}


Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    printf("fd total count %zu\n", channels_.size());

    int numEvents = ::epoll_wait(epollfd_,  &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);

    int savedErrno = errno;

    Timestamp now(Timestamp::now());

    if (numEvents > 0) {
        printf("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);

        // 扩容
        if (implicit_cast<size_t>(numEvents) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents == 0) {
        printf("nothing happend \n");
    } else {
        // error happens, log uncommon ones
        if (savedErrno != EINTR) {
            errno = savedErrno;
            printf("EPollPoller::poll()\n");
        }
    }

    return now;
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    assert(implicit_cast<size_t>(numEvents) <= events_.size());

    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
#ifndef NDEBUG
        int fd = channel->fd();
        ChannelMap::const_iterator it = channels_.find(fd);
        assert(it != channels_.end());
        assert(it->second == channel);
#endif
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

// 新增，修改，删除epoll事件
void EPollPoller::updateChannel(Channel* channel)
{
    Poller::assertInLoopThread();
    const int index = channel->index();
    printf("fd = %d events = %d index = %d\n", channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted) {
        // a new one, add with EPOLL_CTL_ADD
        int fd = channel->fd();
        if (index == kNew) {
            assert(channels_.find(fd) == channels_.end());
            // 建立 fd 和 channel的映射
            channels_[fd] = channel;

        } else { // index == kDeleted
            assert(channels_.find(fd) != channels_.end());
            assert(channels_[fd] == channel);
        }

        // 设置 channel index值，index = kAdded
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);

    } else {
        // update existing one with EPOLL_CTL_MOD/DEL
        int fd = channel->fd();
        (void)fd;
        assert(channels_.find(fd) != channels_.end());
        assert(channels_[fd] == channel);
        assert(index == kAdded);

        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel* channel)
{
    Poller::assertInLoopThread();
    int fd = channel->fd();
    printf("fd = %d\n", fd);

    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(channel->isNoneEvent());

    int index = channel->index();
    assert(index == kAdded || index == kDeleted);

    size_t n = channels_.erase(fd);
    (void)n;
    assert(n == 1);

    if (index == kAdded) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPollPoller::update(int operation, Channel* channel)
{
    struct epoll_event event;
    memZero(&event, sizeof event);
    // 设置 epoll 的配置项
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();
    printf("epoll_ctl op =  %s fd = %d event = { %s }\n", operationToString(operation), fd, channel->eventsToString().c_str());

    /**
     * epollfd_: 参数要操作的文件描述符
     * 
     * operation: 参数则指定操作类型，操作类型有如3种
     *  EPOLL_CTL_ADD：往事件表中注册fd上的事件
     *  EPOLL_CTL_MOD：修改fd上的注册事件
     *  EPOLL_CTL_DEL：删除fd上的注册事件
     */
    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
        if (operation == EPOLL_CTL_DEL) {
            printf("epoll_ctl op = %s fd = %d\n", operationToString(operation), fd);
        } else {
            printf("epoll_ctl op = %s fd = %d\n", operationToString(operation), fd);
        }
    }
}

const char* EPollPoller::operationToString(int op)
{
    switch (op) {
        case EPOLL_CTL_ADD:
            return "ADD";

        case EPOLL_CTL_DEL:
            return "DEL";

        case EPOLL_CTL_MOD:
            return "MOD";

        default:
            assert(false && "ERROR op");
            return "Unknown Operation";
    }
}
