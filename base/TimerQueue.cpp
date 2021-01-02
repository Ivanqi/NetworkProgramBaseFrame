#include "TimerQueue.h"
#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>

int createTimerfd()
{
    /**
     * CLOCK_MONOTONIC：从系统启动这一刻起开始计时,不受系统时间被用户改变的影响
     * TFD_NONBLOCK：非阻塞模式
     * TFD_CLOEXEC：表示当程序执行exec函数时本fd将被系统自动关闭,表示不传递
     */
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0) {
        printf("Failed in timerfd_create\n");
    }
    return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when) 
{
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();

    if (microseconds < 100) {
        microseconds = 100;
    }

    struct timespec ts;
    ts.tv_sec = static_cast<time_t> (microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long> ((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);

    return ts;
}

// 使用epoll监听timer_fd，定时时间到后必须读timerd，不然会一直存在epoll事件，因为timerfd可读
void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    // timerfd读出来的值一般是1，表示超时次数
    ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));

    if (n != sizeof(howmany)) {
        printf("TimerQueue::handleRead() reads %zd bytes instead of 8\n", n);
    }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memZero(&newValue, sizeof(newValue));
    memZero(&oldValue, sizeof(oldValue));

    newValue.it_value = howMuchTimeFromNow(expiration);

    /**
     * int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value)
     * 
     * flags:
     *  0: 相对时间
     *  1: 绝对时间(TFD_TIMER_ABSTIME)
     * 
     * new_value：定时器的超时时间以及超时间隔时间
     * 
     * old_value：如果不为NULL, old_vlaue返回之前定时器设置的超时时间，具体参考timerfd_gettime()函数
     * 
     * 如果flags设置为1，那么epoll监听立马就有事件可读，并且读出的timerfd不是1，因为开机已经过去了很久
     * 如果设置为0，那么会按照设定的时间定第一个定时器，到时后读出的超时次数是1
     */
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret) {
        printf("timerfd_settime() Error:[%d:%s]\n", errno, strerror(errno));
    }
}

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop), timerfd_(createTimerfd()),  timerfdChannel_(loop, timerfd_), 
    timers_(), callingExpiredTimers_(false)
{
    // 设置回调函数
    timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
    // 加入EventLoop的事件列表中
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    // 删除EventLoop的事件列表里的事件
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();

    ::close(timerfd_);

    for (const Entry& timer: timers_) {
        delete timer.second;
    }
}

// 增加定时事件
TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    Timer *timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);
    if (earliestChanged) {
        resetTimerfd(timerfd_, timer->expiration());
    }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());

    // 寻找活跃的定时器
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);

    /**
     * 由于TimerId不负责Timer的生命期，其中保存的Timer* 可能失效，因此不能直接解引用
     * 只有在activeTimers_中找到了Timer时才能提领
     */
    if (it != activeTimers_.end()) {
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1);
        (void)n;
        delete it->first;
        activeTimers_.erase(it);
    } else if (callingExpiredTimers_) { // 自注销。 即在定时回调中注销当前定时器
        cancelingTimers_.insert(timer);
    }

    assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    // 事件读取。不然事件就会堆积
    readTimerfd(timerfd_, now);
    // 获取过期的事件
    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    // 可以安全地在关键区域外回调
    for (const Entry& it : expired) {
        it.second->run();
    }

    callingExpiredTimers_ = false;

    reset(expired, now);
}

// 获取过期事件
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;

    // 当Timestamp相等时会比较Timer*
    // https://www.zhihu.com/question/274643064
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

    // lower_bound()返回的是第一个大于等于x的迭代器
    TimerList::iterator end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now < end->first);

    // 拷贝 begin 到 end 的数据到 expired 的末尾
    std::copy(timers_.begin(), end, back_inserter(expired));
    
    // 删除过期的数据
    timers_.erase(timers_.begin(), end);

    // 在 activeTimers_ 中删除数据
    for (const Entry& it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
        (void) n;
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}

// 对于一些定时任务，重新加入时间队列中
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;

    for (const Entry &it : expired) {
        ActiveTimer timer(it.second, it.second->sequence());
        // 存在定时任务，且不在取消队列中
        if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end()) {
            it.second->restart(now);
            insert(it.second);
        } else {
            delete it.second;
        }
    }

    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid()) {
        resetTimerfd(timerfd_, nextExpire);
    }
}

bool TimerQueue::insert(Timer *timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false;

    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    
    if (it == timers_.end() || when.microSecondsSinceEpoch() > it->first.microSecondsSinceEpoch()) {
        earliestChanged = true;
    }

    {
        std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer));
        assert(result.second);
        (void)result;
    }

    {
        std::pair<ActiveTimerSet::iterator, bool> result = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}