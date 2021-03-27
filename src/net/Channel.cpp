#include "Channel.h"
#include "EventLoop.h"

#include <sstream>
#include <poll.h>
#include <assert.h>
#include <stdio.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fd__)
    :loop_(loop), fd_(fd__), events_(0), revents_(0),
    index_(-1), logHup_(true), tied_(false), eventHandling_(false), addedToLoop_(false)
{
}

Channel::~Channel()
{
    assert(!eventHandling_);
    assert(!addedToLoop_);

    if (loop_->isInLoopThread()) {
        assert(!loop_->hasChannel(this));
    }
}

void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_ = obj;
    tied_ = true;
}

void Channel::update()
{
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove()
{
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

// 事件同一处理方法
void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    eventHandling_= true;
    // POLLIN：有数据可读； POLLHUP: 对方描述符挂起
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) {
        if (logHup_) {
            printf("fd = %d Channel::handle_event() POLLHUP\n", fd_);
        }
        if (closeCallback_) closeCallback_();
    }

    // POLLNVAL: 指定的文件描述符非法
    if (revents_ & POLLNVAL) {
        printf("fd = %d Channel::handle_event() POLLNVAL\n", fd_);
    }

    // POLLERR: 指定的文件描述符发生错误； POLLNVAL: 指定的文件描述符非法
    if (revents_ & (POLLERR | POLLNVAL)) {
        if (errorCallback_) errorCallback_();
    }

    // POLLIN: 有数据可读; POLLPRI: 有紧迫数据可读; POLLRDHUP: 指定的文件描述符挂起事件
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) {
        if (readCallback_) readCallback_(receiveTime);
    }

    // POLLOUT: 写数据不会导致阻塞
    if (revents_ & POLLOUT) {
        if (writeCallback_) writeCallback_();
    }

    eventHandling_ = false;
}

string Channel::reventsToString() const
{
    return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const
{
    return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev)
{
    std::ostringstream oss;
    oss << fd << ": ";

    if (ev & POLLIN) {
        oss << "IN ";
    }
        
    if (ev & POLLPRI) {
        oss << "PRI ";
    }
       
    if (ev & POLLOUT) {
        oss << "OUT ";
    }
        
    if (ev & POLLHUP) {
        oss << "HUP ";
    }
        
    if (ev & POLLRDHUP) {
        oss << "RDHUP ";
    }
        
    if (ev & POLLERR) {
        oss << "ERR ";
    }
        
    if (ev & POLLNVAL) {
        oss << "NVAL ";
    }

    return oss.str();
}