#include "EventLoopThread.h"
#include "EventLoop.h"


EventLoopThread::EventLoopThread(const ThreadInitCallback& cb, const string& name)
    :loop_(NULL), exiting_(false), thread_(std::bind(&EventLoopThread::threadFunc, this), name),
    mutex_(), cond_(mutex_), callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    // 不是100%无竞争，例如threadFunc可能正在运行callback_
    if (loop_ != NULL) {
        /**
         * 如果threadFunc刚刚退出，调用已销毁对象的机会仍然很小
         * 但是，当EventLoopThread析构化时，通常编程无论如何都会退出
         */
        loop_->quit();
        thread_.join();
    }
}

// 启动线程。设置条件变量，进入休眠状态
EventLoop* EventLoopThread::startLoop()
{
    assert(!thread_.started());
    thread_.start();

    EventLoop *loop = NULL;
    {
        MutexLockGuard lock(mutex_);
        while (loop_ == NULL) {
            cond_.wait();
        }
        loop = loop_;
    }

    return loop;
}

void EventLoopThread::threadFunc()
{
    EventLoop loop;
    if (callback_) {
        callback_(&loop);
    }

    // 设置线程loop。然后唤醒线程
    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;
        cond_.notify();
    }

    loop.loop();
    MutexLockGuard lock(mutex_);
    loop_ = NULL;
}