#include "TcpConnection.h"
#include "WeakCallback.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include "SocketsOps.h"

#include <stdio.h>
#include <errno.h>

void defaultConnectionCallback(const TcpConnectionPtr& conn)
{
    string connectState = conn->connected() ? "UP" : "DOWN";
    printf("%s -> %s is %s\n", conn->localAddress().toIpPort().c_str(), conn->peerAddress().toIpPort().c_str(), connectState.c_str());
}

void defaultMessageCallback(const TcpConnectionPtr& conn, Buffer *buf, Timestamp)
{
    buf->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop *loop, const string& nameArg, int sockfd, const InetAddress& localAddr, const InetAddress& peerAddr)
    :loop_(loop), name_(nameArg), state_(kConnecting), reading_(true), 
    socket_(new Socket(sockfd)), channel_(new Channel(loop, sockfd)), 
    localAddr_(localAddr), peerAddr_(peerAddr), highWaterMark_(64 * 1024 * 1024)
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, _1));

    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));

    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));

    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    printf("TcpConnection::~TcpConnection::dtor[ %s ] fd= %d state= %s\n", name_.c_str(), channel_->fd(), stateToString());
    assert(state_ == kDisconnected);
}

string TcpConnection::getTcpInfoString() const
{
    char buf[1024];
    buf[0] = '\0';
    socket_->getTcpInfoString(buf, sizeof(buf));
    return buf;
}

void TcpConnection::send(const void *data, int len)
{
    send(StringPiece(static_cast<const char*>(data), len));
}

/**
 * 如果在非IO线程调用，它会把message复制一份，传给IO线程中的sendInLoop()来发送
 * 这样做或许有轻微的效率损失，但是线程安全性很容易验证
 */
void TcpConnection::send(const StringPiece& message)
{
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);
        } else {
            void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(
                std::bind(fp, this, message.as_string())
            );
        }
    }
}

void TcpConnection::send(Buffer *buf)
{
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        } else {
            void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(std::bind(fp, this, buf->retrieveAllAsString()));
        }
    }
}

void TcpConnection::sendInLoop(const StringPiece& message)
{
    sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void* data, size_t len)
{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    if (state_ == kDisconnected) {
        return ;
    }

    // 如果输出队列中没有任何内容，请尝试直接写入
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = sockets::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            // 如果全部发送完毕，就触发写入完成的回调
            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultError = true;
                }
            }
        }
    }

    assert(remaining <= len);
    // 如果没有全部发送完成
    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        // 高水位回调
        if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }

        // 添加到缓冲区。因为outputBuffer_已经有待发送的数据，那么就不能先尝试发送了，因为这会造成数据乱序
        outputBuffer_.append(static_cast<const char *>(data) + nwrote, remaining);

        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{
    // 使用比较和交换
    if (state_ == kConnected) {
        setState(kDisconnected);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::forceClose()
{
    // 使用比较和交换
    if (state_ == kConnected || state_ == kDisconnected) {
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseWithDelay(double seconds)
{
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        // 不强制关闭环路以避免竞争条件
        loop_->runAfter(seconds, makeWeakCallback(shared_from_this(), &TcpConnection::forceClose));
    }
}

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting) {
        handleClose();
    }
}

const char* TcpConnection::stateToString() const
{
    switch (state_) {
        case kDisconnected:
            return "kDisconnected";
        
        case kConnecting:
            return "kConnecting";
        
        case kConnected:
            return "kConnected";

        case kDisconnecting:
            return "kDisconnecting";

        default:
            return "unknown state";
    }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead()
{
    loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
}

void TcpConnection::startReadInLoop()
{
    loop_->assertInLoopThread();
    if (!reading_ || !channel_->isReading()) {
        channel_->enableReading();
        reading_ = true;
    }
}

void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();

        connectionCallback_(shared_from_this());
    }
    
    channel_->remove();
}

// 读取对端发送的消息。 把readable事件通过MessageCallback传达给客户
void TcpConnection::handleRead(Timestamp receiveTime)
{
    loop_->assertInLoopThread();
    int saveErrno = 0;

    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {
        handleClose();
    } else {
        errno = saveErrno;
        handleError();
    }
}

/**
 * 往对端写入消息.  自己处理writeable事件
 * 当socket变得可写时，Channel会调用TcpConnection::handleWrite()，这里会继续发送outputBuffer_中的数据
 * 一旦发送完毕，立刻停止观察writeable事件，避免busy loop
 * 另外如果这时连接正在关闭，则调用shutdownInLoop()，继续执行关闭过程
 * 这里不需要处理错误，因为一旦发生错误，handleRead()会读到0字节，继而关闭连接
 */
void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting()) {
        ssize_t n = sockets::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());

        if (n > 0) {
            outputBuffer_.retrieve(0);
            if (outputBuffer_.readableBytes() == 0) {
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }

                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            printf("TcpConnection::handleWrite\n");
        }
    } else{
        printf("Connection fd =  %d is down, no more writing\n", channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnected || state_ == kDisconnecting);

    // 我们不关闭fd，把它交给dtor，这样我们可以很容易地找到泄漏
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis);

    closeCallback_(guardThis);
}

// 输出错误信息
__thread char t_errnobuf[512];
void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
    const char *errmsg = strerror_r(err, t_errnobuf, sizeof(t_errnobuf));
    printf("TcpConnection::handleError [ %s ] -  - SO_ERROR = %d, %s", name_.c_str(), err, errmsg);
}
