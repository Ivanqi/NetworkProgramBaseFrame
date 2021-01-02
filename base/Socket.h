#ifndef EVENT_SOCKET_H
#define EVENT_SOCKET_H

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

class InetAddress;

/**
 * 套接字文件描述符的包装. RAII handle，封装了socket文件描述符的生命期
 * 
 * 停止时关闭sockfd
 * 它是线程安全的，所有操作都是通过操作系统来完成的
 */
class Socket
{
    private:
        const int sockfd_;

    public:
        explicit Socket(int sockfd): sockfd_(sockfd)
        {
        }

        ~Socket();

        int fd() const
        {
            return sockfd_;
        }

        // 返回true表示成功
        bool getTcpInfo(struct tcp_info*) const;
        bool getTcpInfoString(char *buf, int len) const;

        // 如果该地址正在使用，则中止
        void bindAddress(const InetAddress &localaddr);
        void listen();

        /**
         * 成功时，返回一个非负数，该整数是接受的套接字的描述符，该套接字已设置  non-blocking并在exec关闭
         * 出错时，返回-1，而peeraddr保持不变
         */
        int accept(InetAddress *perraddr);

        void shutdownWrite();

        // 启用/禁用TCP_NODELAY（禁用/启用Nagle算法）
        void setTcpNoDelay(bool on);

        // 启用/禁用 SO_REUSEADDR
        // TCP套接字处于TIME_WAIT，这个时候SO_RESUADDR就可以起作用，让端口重复绑定适用
        void setReuseAddr(bool on);

        // 启用/禁用 SO_REUSEPORT
        // SO_REUSEPORT 支持多个进程或线程绑定同一个端口，提高服务器性能
        void setReusePort(bool on);

        // 启动/禁用 SO_KEEPALIVE
        // SO_KEEPALIVE 发送周期性保活报文以维持连接
        void setKeepAlive(bool on);
};

#endif