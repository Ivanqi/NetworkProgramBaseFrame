#include "EventLoop.h"
#include "MutexLock.h"
#include "Channel.h"
#include "Poller.h"
#include "SocketsOps.h"
#include "TimerQueue.h"

#include <algorithm>
#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <stdio.h>

__thread EventLoop *t_loopInThisThread = 0;

const int kPollTimeMs = 10000;

int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        printf("Failed in eventfd\n");
        abort();
    }
    return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
    public:
        IgnoreSigPipe()
        {
            ::signal(SIGPIPE, SIG_IGN);
        }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;

/**
 * 每个线程至多有一个EventLoop对象，那么使用getEventLoopOfCurrentThread返回这个对象
 * 如果当前线程不是IO线程，返回值为NULL
 */
EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
    return t_loopInThisThread;   
}

/**
 * EventLoop的构造函数会记住本对象所属的线程(threadId_)
 * 创建了EventLoop对象的线程是IO线程，其主要功能是运行事件循环
 */
EventLoop::EventLoop()
    : looping_(false), quit_(false), eventHandling_(false), iteration_(0),
    threadId_(CurrentThread::tid()), poller_(Poller::newDefaultPoller(this)),
    timerQueue_(new TimerQueue(this)), wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)), currentActiveChannel_(NULL)
{
    printf("wakeupFd_:%d\n", wakeupFd_);
    if (t_loopInThisThread) {
        printf("t_loopInThisThread is not null, exists in this thread: %d\n", threadId_);
    } else {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 初始化poller
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = NULL;
}

void EventLoop::loop()
{
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        activeChannels_.clear();
        // 监听文件描述符注册的事件
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        ++iteration_;
        eventHandling_ = true;
        for (Channel *channel: activeChannels_) {
            currentActiveChannel_ = channel;
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }

        currentActiveChannel_ = NULL;
        eventHandling_ = false;
        // 队列事件触发
        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::quit()
{
    quit_ = true;
    /**
     * 有可能loop()在while(!quit)执行的时候退出，然后EventLoop析构函数，然后访问一个无效的对象
     * 可以在这两个位置使用互斥体修复
     */
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    // 减少锁范围
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

size_t EventLoop::queueSize() const
{
    MutexLockGuard lock(mutex_);
    return pendingFunctors_.size();
}

// 在指定的时间调用TimerCallback
TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
{
    return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

// 等一段时间调用TimerCallback
TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb));
}

// 以固定的间隔反复调用TimerCallback
TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(std::move(cb), time, interval);
}

// 取消timer
void EventLoop::cancel(TimerId timerId)
{
    return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    if (eventHandling_) {
        assert(currentActiveChannel_ == channel || 
            std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
    }

    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
    printf("EventLoop::abortNotInLoopThread - was created in threadId_ =  %d, current thread id = %d\n", threadId_, CurrentThread::tid());
}

/**
 * 往wakeupFd_写入内容.唤醒wakeupFd_句柄
 * wakeup() 的过程本质上是对这个eventfd进行写操作，以触发eventfd的可读事件
 * 这样起到了唤醒EventLoop的作用
 */
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = sockets::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        printf("EventLoop::wakeup() writes %zd bytes instead of 8\n", n);
    }
}

// 从wakeupFd_读出内容
void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = sockets::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        printf("EventLoop::handleRead() reads %zd bytes instead of 8\n", n);
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    // 缩小锁范围
    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (const Functor& functor: functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
    for (const Channel *channel: activeChannels_) {
        printf("{ %s }\n", channel->reventsToString().c_str());
    }
}