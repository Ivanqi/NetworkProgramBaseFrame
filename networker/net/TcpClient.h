#ifndef NETWORKER_NET_TCPCLIENT_H
#define NETWORKER_NET_TCPCLIENT_H

#include "networker/base/MutexLock.h"
#include "networker/net/TcpConnection.h"

namespace networker
{
namespace net
{
    class Connector;
    typedef std::shared_ptr<Connector> ConnectorPtr;

    /**
     * TcpClient具备TcpConnection断开重连的功能，加上Connector具有反复尝试连接的功能，因此客户端和服务端的启动顺序无关紧要
     */
    class TcpClient
    {
        private:
            EventLoop *loop_;   // loop
            ConnectorPtr connector_;    // 避免露出Connector
            const string name_; // tcpclient 名称         
            ConnectionCallback connectionCallback_; // connection回调
            MessageCallback messageCallback_;   // read回调
            WriteCompleteCallback writeCompleteCallback_;   // 写完成回调

            bool retry_;    // atomic, 重连
            bool connect_;  // atomic

            // always in loop thread
            int nextConnId_;
            mutable MutexLock mutex_;

            TcpConnectionPtr connection_;

        public:
            TcpClient(EventLoop *loop, const InetAddress& serverAddr, const string& nameArg);

            ~TcpClient();

            void connect();

            void disconnect();

            void stop();

            TcpConnectionPtr connection() const
            {
                MutexLockGuard lock(mutex_);
                return connection_;
            }

            EventLoop *getLoop() const
            {
                return loop_;
            }

            bool retry() const
            {
                return retry_;
            }

            void enableRetry()
            {
                retry_ = true;
            }

            const string& name() const
            {
                return name_;
            }

            // 设置connection 回调。不是线程安全
            void setConnectionCallback(ConnectionCallback cb)
            {
                connectionCallback_ = std::move(cb);
            }

            // 设置message回调。不是线程安全
            void setMessageCallback(MessageCallback cb)
            {
                messageCallback_ = std::move(cb);
            }

            // 设置 write complete 回调。不是线程安全
            void setWriteCompleteCallback(WriteCompleteCallback cb)
            {
                writeCompleteCallback_ = std::move(cb);
            }
        
        private:
            // Not thread safe, but in loop
            void newConnection(int sockfd);

            // Not thread safe, but in loop
            void removeConnection(const TcpConnectionPtr& conn);
    };
};
};

#endif