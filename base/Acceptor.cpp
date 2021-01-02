#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "SocketsOps.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

Acceptor::Acceptor(EventLoop *loop, const InetAddress& listenAddr, bool reuseport)
    :loop_(loop),  acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())), 
    acceptChannel_(loop, acceptSocket_.fd()), listenning_(false), idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >= 0);
    
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    
    // 设置channel 的 POLLIN | POLLPRI | POLLRDHU 事件回调函数
    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this)
    );
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}

void Acceptor::listen()
{
    loop_->assertInLoopThread();
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

// 接受客户端的连接，并回调用户callback
void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr;

    int connfd = acceptSocket_.accept(&peerAddr);
    /*
        这里没有考虑了文件描述符耗尽的情况，在拿到大于等于0的connfd之后，非阻塞poll(2)一下，看fd是否可读写
        正常情况下poll(2)会返回writeable，表明connfd可用
        如果poll(2)返回错误，表明connfd有问题，应该立刻关闭连接
     */
    if (connfd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connfd, peerAddr);
        } else {
            sockets::close(connfd);
        }
    } else {
        if (errno == EMFILE) {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
