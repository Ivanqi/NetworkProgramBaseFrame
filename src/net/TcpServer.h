#ifndef EVENT_TCPSERVER_H
#define EVENT_TCPSERVER_H

#include "Atomic.h"
#include "Types.h"
#include "TcpConnection.h"
#include <boost/noncopyable.hpp>

#include <map>

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

/**
 * TCP服务器，支持单线程和线程池模型
 */
class TcpServer: boost::noncopyable
{
    public:
        typedef std::function<void(EventLoop*)> ThreadInitCallback;

        enum Option {
            kNoReusePort, kReusePort
        };

    private:
        /*
            TcpServer持有目前存活的TcpConnection的shared_ptr(定义为TcpConnectionPtr)
            因为TcpConnection 对象的生命期是模糊的，用户也可以持有TcpConnectionPtr

            每个TcpConnection对象有一个名字，这个名字是由其所属的TcpServer在创建TcpConnection对象时生成的，名字为ConectionMap的key
         */
        typedef std::map<string, TcpConnectionPtr> ConectionMap;

        EventLoop* loop_;   // the acceptor loop

        const string ipPort_;

        const string name_;

        std::unique_ptr<Acceptor> acceptor_;    // 避免暴露Acceptor， 使用Acceptor来获取新连接的fd

        std::shared_ptr<EventLoopThreadPool> threadPool_;

        ConnectionCallback connectionCallback_;

        MessageCallback messageCallback_;

        WriteCompleteCallback writeCompleteCallback_;
        
        ThreadInitCallback threadInitCallback_;

        AtomicInt32 started_;

        int nextConnId_;    // 连接客户端数量

        ConectionMap connections_;

    public:
        TcpServer(EventLoop *loop, const InetAddress& listenAddr, const string& nameArg, Option option = kNoReusePort);

        ~TcpServer();

        const string& ipPort() const
        {
            return ipPort_;
        }

        const string& name() const
        {
            return name_;
        }

        EventLoop* getLoop() const
        {
            return loop_;
        }

        /**
         * 设置处理输入的线程数
         * 
         * 总是接受循环线程中的新连接
         * 必须在 start函数之前调用
         * 
         * @param numThreads
         *  0 表示循环线程中的所有I/O，不会创建线程。这个是默认值
         *  1 表示另一个线程中的所有I/O
         *  N 表示有N个线程的线程池，新的连接按循环分配
         */
        void setThreadNum(int numThreads);

        void setThreadInitCallback(const ThreadInitCallback& cb)
        {
            threadInitCallback_ = cb;
        }

        // 调用start函数之后生效
        std::shared_ptr<EventLoopThreadPool> threadPool()
        {
            return threadPool_;
        }

        /**
         * 如果服务器没有侦听，则启动服务器
         * 多次调用是无损的
         * 线程安全
         */
        void start();

        /**
         * 设置连接回调
         * 不是线程安全
         */
        void setConnectionCallback(const ConnectionCallback& cb)
        {
            connectionCallback_ = cb;
        }

        /**
         * 设置消息回调
         * 不是线程安全
         */
        void setMessageCallback(const MessageCallback& cb)
        {
            messageCallback_ = cb;
        }

        /**
         * 设置写入完成回调
         * 不是线程安全
         */
        void setWriteCompleteCallback(const WriteCompleteCallback& cb)
        {
            writeCompleteCallback_ = cb;
        }
    
    private:
        // 不是线程安全的，而是在循环
        void newConnection(int sockfd, const InetAddress& peerAddr);

        // 线程安全
        void removeConnection(const TcpConnectionPtr& conn);

        // 不是线程安全的，而是在循环
        void removeConnectionInLoop(const TcpConnectionPtr& conn);
};

#endif