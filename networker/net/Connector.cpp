#include "networker/net/Connector.h"
#include "networker/net/Channel.h"
#include "networker/net/EventLoop.h"
#include "networker/net/SocketsOps.h"
#include "networker/base/Logging.h"

#include <errno.h>
#include <stdio.h>

using namespace networker;
using namespace networker::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop *loop, const InetAddress& serverAddr)
    : loop_(loop), serverAddr_(serverAddr), connect_(false), state_(kDisconnected), retryDelayMs_(kInitRetryDelayMs)
{
    LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
    LOG_DEBUG << "dtor[" << this << "]";
    assert(!channel_);
}

void Connector::start()
{
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this));
}

void Connector::startInLoop()
{
    loop_->assertInLoopThread();
    assert(state_ == kDisconnected);
    if (connect_) {
        connect();
    } else {
        LOG_DEBUG << "do not connect";
    }
}

void Connector::stop()
{
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));    // FIXME: unsafe
}

void Connector::stopInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == kConnecting) {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

/**
 * socket 是一次性的，一旦出错(比如对方拒绝连接)，就无法恢复，只能关闭重来
 * 但Connector 是可以反复使用的，因此每次尝试连接都要使用新的socket文件描述符和新的Channel对象
 * 要留意Channel对象的生命期管理，并防止socket文件描述符符泄漏
 */
void Connector::connect()
{
    // 创建一个sock，然后去连接对端
    int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
    int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());
    int savedErrno = (ret == 0) ? 0 : errno;

    switch (savedErrno) {
        /**
         * '正在连接'的返回码是EINPROGRESS
         * 另外，即便出现socket可写，也不一定意味着连接已成功建立，还需要用 getsockopt(sockfd, SOL_SOCKET, SO_ERROR, ...)再确定一次
         */
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            connecting(sockfd);
            break;
        
        /**
         * EAGAIN是真的错误，表明本机 ephemeral port 暂时用完，关闭socket再延期重试
         */
        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry(sockfd);
            break;
        
        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
            sockets::close(sockfd);
            break;
        
        default:
            LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
            sockets::close(sockfd);
            break;
    }
}

void Connector::restart()
{
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

void Connector::connecting(int sockfd)
{
    setState(kConnecting);  // 设置状态
    assert(!channel_);

    channel_.reset(new Channel(loop_, sockfd)); // 重新设置channel

    channel_->setWriteCallback(std::bind(&Connector::handleWrite, this)); // FIXME: unsafe

    channel_->setErrorCallback(std::bind(&Connector::handleError, this)); // FIXME: unsafe

    channel_->enableWriting(); // 新增/修改 epoll/poll事件
}

// 删除和重置channle
int Connector::removeAndResetChannel()
{
    channel_->disableAll();
    channel_->remove();

    int sockfd = channel_->fd();
    // Can't reset channel_ here, because we are inside Channel::handleEvent
    loop_->queueInLoop(std::bind(&Connector::resetChannel, this));  // FIXME: unsafe
    return sockfd;
}

void Connector::resetChannel()
{
    channel_.reset();
}

void Connector::handleWrite()
{
    LOG_TRACE << "Connector::handleWrite " << state_;

    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);

        if (err) {
            LOG_TRACE << "Connector::handleWrite - SO_ERROR = " << err << " " << strerror_tl(err);
            retry(sockfd);

        } else if (sockets::isSelfConnect(sockfd)) {
            LOG_TRACE << "Connector::handleWrite - Self connect";
            retry(sockfd);

        } else {
            setState(kConnected);            
            if (connect_) {
                newConnectionCallback_(sockfd);
            } else {
                sockets::close(sockfd);
            }
        }
    } else {
        // what happened?
        assert(state_ == kDisconnected);
    }
}

void Connector::handleError()
{
    LOG_ERROR << "Connector::handleError state= " << state_;

    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
        retry(sockfd);
    }
}

void Connector::retry(int sockfd)
{
    sockets::close(sockfd);
    setState(kDisconnected);

    if (connect_) {
        LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort() << " in " << retryDelayMs_ << " milliseconds. ";
        /**
         * 重试间隔应该应该逐渐延长，例如0.5s, 1s, 2s, 4s,直至30s,即back-off
         */
        loop_->runAfter(retryDelayMs_ / 1000.0, std::bind(&Connector::startInLoop, shared_from_this()));

        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    } else {
        LOG_DEBUG << "do not connect";
    }
}