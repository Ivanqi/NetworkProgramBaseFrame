    #include "networker/net/Socket.h"
    #include "networker/net/InetAddress.h"
    #include "networker/net/SocketsOps.h"
    #include "networker/base/Logging.h"

    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <stdio.h>  // snprintf

    using namespace networker;
    using namespace networker::net;

    Socket::~Socket()
    {
        sockets::close(sockfd_);
    }

    bool Socket::getTcpInfo(struct tcp_info *tcpi) const
    {
        socklen_t len = sizeof(*tcpi);
        memZero(tcpi, len);
        return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
    }

    bool Socket::getTcpInfoString(char *buf, int len) const
    {
        struct tcp_info tcpi;
        bool ok = getTcpInfo(&tcpi);

        if (ok) {
            snprintf(buf, len, "unrecovered=%u "
                "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
                "lost=%u retrans=%u rtt=%u rttvar=%u "
                "sshthresh=%u cwnd=%u total_retrans=%u",
                tcpi.tcpi_retransmits,      // 未恢复的[RTO]超时数， 超时重传的次数
                tcpi.tcpi_rto,              // 超时时间，单位为微秒
                tcpi.tcpi_ato,              // 延时确认的估值，单位为微秒
                tcpi.tcpi_snd_mss,          // 本端的MSS
                tcpi.tcpi_rcv_mss,          // 对端的MSS
                tcpi.tcpi_lost,             // 丢失且未恢复的数据段数
                tcpi.tcpi_retrans,          // 重传且未确认的数据段数
                tcpi.tcpi_rtt,              // 平滑的往返时间，单位为微秒
                tcpi.tcpi_rttvar,           // 四分之一mdev，单位为微秒
                tcpi.tcpi_snd_ssthresh,     // 慢启动阈值
                tcpi.tcpi_snd_cwnd,         // 拥塞窗口
                tcpi.tcpi_total_retrans);   // 本连接的总重传个数
        }
        return ok;
    }

    void Socket::bindAddress(const InetAddress& addr)
    {
        sockets::bindOrDie(sockfd_, addr.getSockAddr());
    }

    void Socket::listen()
    {
        sockets::listenOrDie(sockfd_);
    }

    int Socket::accept(InetAddress *peeraddr)
    {
        struct sockaddr_in6 addr;
        memZero(&addr, sizeof(addr));

        int connfd = sockets::accept(sockfd_, &addr);
        if (connfd >= 0) {
            peeraddr->setSockAddrInet6(addr);
        }

        return connfd;
    }

    void Socket::shutdownWrite()
    {
        printf("Socket::shutdownWrite:%d \n", sockfd_);
        sockets::shutdownWrite(sockfd_);
    }

    /**
     * 尽可能发送大块数据，避免网络中充斥着许多小数据块
     * TCP_NODELAY可以解决Nagle算法带来的问题，开启TCP_NODELAY意味着允许小包的发送且不强制等待，对时效高且数据量小的应用非常实用
     * 从应用程序的角度来说应该尽量避免写小包，从而实现数据包大小和数据包数量的效率最大化
     */
    void Socket::setTcpNoDelay(bool on)
    {
        int optval = on ? 1 : 0;
        ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof(optval)));
    }

    /**
     * SO_REUSEPORT和SO_REUSEADDR 的区别
     *  1. SO_REUSEPORT是端口重用。SO_REUSEADDR是地址重用
     *  2. SO_REUSEPORT是允许多个socket绑定到同一个IP + PORT上。SO_REUSEADDR用于对TCP套接字处于TIME_WAIT状态下的socket，才可以重复绑定使用
     *  3. 两者使用场景完全不同
     *      1. SO_REUSEADDR 这个套接字选项通知内核，如果端口忙，但TCP状态处于TIME_WAIT，可以重用端口
     *          这个一般用于当你的程序停止后想立即启动的时候，如果没有这个选项，会报错EADDRINUSE
     *          需要等到TIME_WAIT结束才能重新绑定到同一个IP + PORT上
     *      2. SO_REUSEPORT 用于多核环境下，允许多个线程或者进程监听同一个IP + PORT，无论UDP、TCP
     */


    /**
     * TCP套接字处于TIME_WAIT，这个时候SO_RESUADDR就可以起作用，让端口重复绑定适用
     */
    void Socket::setReuseAddr(bool on)
    {
        int optval = on ? 1 : 0;
        ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof(optval)));
    }

    // SO_REUSEPORT 支持多个进程或线程绑定同一个端口，提高服务器性能
    void Socket::setReusePort(bool on)
    {
    #ifdef SO_REUSEPORT
        int optval = on ? 1 : 0;
        int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof(optval)));

        if (ret < 0 && on) {
            LOG_SYSERR << "SO_REUSEPORT failed.";
        }
    #else
        if (on) {
            LOG_SYSERR << "SO_REUSEPORT is not supported.";
        }
    #endif
    }

    // SO_KEEPALIVE 发送周期性保活报文以维持连接
    void Socket::setKeepAlive(bool on)
    {
        int optval = on ? 1 : 0;
        ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof(optval)));
    }