#include "networker/net/EventLoopThread.h"
#include "networker/net/EventLoop.h"

using namespace networker;
using namespace networker::net;

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

/**
 * 线程主函数在stack上定义EventLoop对象，然后将其地址赋值给loop_成员变量
 * 最后notify()条件变量，唤醒startLoop()
 * 
 * 由于EventLoop的生命期与线程主函数的作用域相同，因此在threadFunc()退出之后这个指针就失效了
 */
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