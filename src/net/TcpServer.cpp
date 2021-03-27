#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "SocketsOps.h"

#include <stdio.h>  // snprintf

TcpServer::TcpServer(EventLoop *loop, const InetAddress& listenAddr, const string& nameArg, Option option)
    :loop_(loop), ipPort_(listenAddr.toIpPort()), name_(nameArg),
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
    threadPool_(new EventLoopThreadPool(loop, name_)),
    connectionCallback_(defaultConnectionCallback), messageCallback_(defaultMessageCallback),
    nextConnId_(1)
{
    // 设置 socket accept 的执行函数
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();

    for (auto& item: connections_) {
        TcpConnectionPtr conn(item.second);
        item.second.reset();

        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if (started_.getAndSet(1) == 0) {
        threadPool_->start(threadInitCallback_);

        assert(!acceptor_->listenning());

        loop_->runInLoop(
            std::bind(&Acceptor::listen, get_pointer(acceptor_))
        );
    }
}

/**
 * @param int sockfd 对端的的文件描述符
 * @param const InetAddress& peerAddr 对端的IP地址
 * 
 * 所有对IO和buffer的读写，都应该在IO线程中完成
 * 
 * 一般情况下，先把交给Worker线程池之前，应该现在IO线程中把Buffer进行切分解包等动作
 * 将解包后的消息交由线程池处理，避免多个线程操作同一个资源
 */
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 在一个IO线程中
    loop_->assertInLoopThread();
    // 获取一个io线程
    EventLoop *ioLoop = threadPool_->getNextLoop();

    char buf[64];
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;

    string connName = name_ + buf;

    InetAddress localAddr(sockets::getLocalAddr(sockfd));

    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));

    connections_[connName] = conn;

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    
    // 线程不安全
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, _1));

    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

/**
 * TcpServer::removeConnection() 把conn从ConnectionMap中移除
 * 这时TcpConnection已经是命悬一线，如果用户不持有TcpConnectionPtr的话，conn的引用计数已降到1
 * 这里一定要用 EventLoop::queueInLoop(), 否则就会出现生命周期管理问题
 * 另外注意这里用 std::bind 让TcpConnection的生命期长到调用connectDestoryed()的时刻
 */
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    loop_->assertInLoopThread();
    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n == 1);
    EventLoop* ioLoop = conn->getLoop();

    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}