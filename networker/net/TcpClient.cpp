#include "networker/net/TcpClient.h"
#include "networker/net/Connector.h"
#include "networker/net/EventLoop.h"
#include "networker/net/SocketsOps.h"
#include "networker/base/Logging.h"

#include <stdio.h>  // snprintf
#include <functional>


using namespace networker;
using namespace networker::net;
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

namespace networker
{
namespace net
{
namespace detail
{
    void removeConnection(EventLoop *loop, const TcpConnectionPtr& conn)
    {
        loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }

    void removeConnector(const ConnectorPtr& connector) 
    {

    }
}
};
};

TcpClient::TcpClient(EventLoop *loop, const InetAddress& serverAddr, const string& nameArg)
    :loop_(loop), connector_(new Connector(loop, serverAddr)), name_(nameArg),
    connectionCallback_(defaultConnectionCallback), messageCallback_(defaultMessageCallback),
    retry_(false), connect_(true), nextConnId_(1)
{
    connector_->setNewConnectionCallback(std::bind(&TcpClient::newConnection, this, _1));
}

TcpClient::~TcpClient()
{
    TcpConnectionPtr conn;
    bool unique = false;
    {
        MutexLockGuard lock(mutex_);
        // 防止connection_被其他线程修改
        unique = connection_.unique();
        conn = connection_;
    }

    if (conn) {
        assert(loop_ == conn->getLoop());
        // FIXME: not 100% safe, if we are in different thread
        CloseCallback cb = std::bind(&detail::removeConnection, loop_, _1);
        loop_->runInLoop(std::bind(&TcpConnection::setCloseCallback, conn, cb));

        if (unique) {
            conn->forceClose();
        }
    } else {
        connector_->stop();
        loop_->runAfter(1, std::bind(&detail::removeConnector, connector_));
    }
}

void TcpClient::connect()
{
    LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to " << connector_->serverAddress().toIpPort();
    connect_ = true;
    connector_->start();
}

void TcpClient::disconnect()
{
    connect_ = false;
    {
        MutexLockGuard lock(mutex_);
        if (connection_) {
            connection_->shutdown();
        }
    }
}

void TcpClient::stop()
{
    connect_ = false;
    connector_->stop();
}

void TcpClient::newConnection(int sockfd)
{
    loop_->assertInLoopThread();
    InetAddress peerAddr(sockets::getPeerAddr(sockfd));

    char buf[32];
    snprintf(buf, sizeof(buf), ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
    ++nextConnId_;

    string connName = name_ + buf;

    InetAddress localAddr(sockets::getLocalAddr(sockfd));

    TcpConnectionPtr conn(new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));

    conn->setConnectionCallback(connectionCallback_);

    conn->setMessageCallback(messageCallback_);

    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 不是线程安全
    conn->setCloseCallback(std::bind(&TcpClient::removeConnection, this, _1));

    {
        MutexLockGuard lock(mutex_);
        connection_ = conn;
    }

    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->assertInLoopThread();
    assert(loop_ == conn->getLoop());

    {
        MutexLockGuard lock(mutex_);
        assert(connection_ == conn);
        connection_.reset();
    }

    loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    if (retry_ && connect_) {
        LOG_INFO << "TcpClient::connect[" << name_ << "] - Reconnecting to " << connector_->serverAddress().toIpPort();
        connector_->restart();
    }
}