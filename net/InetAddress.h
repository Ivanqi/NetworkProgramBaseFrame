#ifndef EVENT_INTERADDRESS_H
#define EVENT_INTERADDRESS_H

#include "StringPiece.h"
#include "SocketsOps.h"
#include <netinet/in.h>

class InetAddress
{
    private:
        union {
            struct sockaddr_in addr_;
            struct sockaddr_in6 addr6_;
        };

    public:
        // 构造具有给定端口号的端口
        // 主要用于TcpServer监听
        explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false, bool ipv6 = false);

        // 构造具有给定IP和端口的端点
        // IP 地址类型 "1.2.3.4"
        InetAddress(StringArg ip, uint16_t port, bool ipv6 = false);

        // 使用给定的 struct sockaddr_in构造端点
        // 主要用于接受新的连接
        explicit InetAddress(const struct sockaddr_in& addr): addr_(addr) {}

        explicit InetAddress(const struct sockaddr_in6& addr): addr6_(addr) {}

        sa_family_t family() const
        {
            return addr_.sin_family;
        }

        string toIp() const;

        string toIpPort() const;

        uint16_t toPort() const; 

        const struct sockaddr* getSockAddr() const
        {
            return sockets::sockaddr_cast(&addr6_);
        }

        void setSockAddrInet6(const struct sockaddr_in6& addr6)
        {
            addr6_ = addr6;
        }

        uint32_t ipNetEndian() const;

        uint16_t portNetEndian() const 
        {
            return addr_.sin_port;
        }

        /**
         * 将主机名解析为IP地址，而不是更改端口或sin_系列
         * 成功时返回true
         * 线程安全的
         */
        static bool resolve(StringArg hostname, InetAddress* result);

        // set IPv6 ScopeID
        void setScopeId(uint32_t scope_id);
};

#endif