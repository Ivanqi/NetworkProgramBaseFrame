#ifndef EVENT_CONNECTOR_H
#define EVENT_CONNECTOR_H


#include "InetAddress.h"
#include <boost/noncopyable.hpp>

#include <functional>
#include <memory>

class Channel;
class EventLoop;

class Connector : boost::noncopyable, public std::enable_shared_from_this<Connector>
{
    public:
        typedef std::function<void (int sockfd)> NewConnectionCallback;
    
    private:
        enum States {kDisconnected, kConnecting, kConnected};   // 未连接，连接中，已连接
        static const int kMaxRetryDelayMs = 30 * 1000;
        static const int kInitRetryDelayMs = 500;

        EventLoop *loop_;
        InetAddress serverAddr_;
        bool connect_;  // atomic
        States state_;
        std::unique_ptr<Channel> channel_;
        NewConnectionCallback newConnectionCallback_;
        int retryDelayMs_;
    
    public:
        Connector(EventLoop *loop, const InetAddress& serverAddr);

        ~Connector();

        void setNewConnectionCallback(const NewConnectionCallback& cb)
        {
            newConnectionCallback_ = cb;
        }

        const InetAddress& serverAddress() const 
        { 
            return serverAddr_;
        }

        void start();   // 可以在任何线程中调用

        void restart(); // 必须在loop线程中调用

        void stop();   // 可以在任何线程中调用

    private:
        void setState(States s)
        {
            state_ = s;
        }

        void startInLoop();

        void stopInLoop();

        void connect();

        void connecting(int sockfd);

        void handleWrite();

        void handleError();

        void retry(int sockfd);

        int removeAndResetChannel();

        void resetChannel();
};

#endif