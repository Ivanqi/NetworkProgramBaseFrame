#include "InetAddress.h"
#include "Endian.h"
#include "SocketsOps.h"

#include <netdb.h>
#include <netinet/in.h>
#include <assert.h>
#include <stddef.h>

#pragma GCC diagnostic ignored "-Wold-style-cast"

// INADDR_ANY 绑定地址 0.0.0.0上监听，能接收任意一块网卡的连接
static const in_addr_t kInaddrAny = INADDR_ANY;

// INADDR_LOOPBACK 绑定地址LOOPBAC，往往是127.0.0.1，只能接收127.0.0.1上面的连接请求
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"


static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6),
              "InetAddress is same size as sockaddr_in6");
              
static_assert(offsetof(sockaddr_in, sin_family) == 0, "sin_family offset 0");

static_assert(offsetof(sockaddr_in6, sin6_family) == 0, "sin6_family offset 0");

static_assert(offsetof(sockaddr_in, sin_port) == 2, "sin_port offset 2");

static_assert(offsetof(sockaddr_in6, sin6_port) == 2, "sin6_port offset 2");


InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6)
{
    static_assert(offsetof(InetAddress, addr6_) == 0, "addr6_ offset 0");
    static_assert(offsetof(InetAddress, addr_) == 0, "addr_ offset 0");

    if (ipv6) {
        memZero(&addr6_, sizeof(addr6_));
        addr6_.sin6_family = AF_INET6;
        in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
        addr6_.sin6_addr = ip;
        addr6_.sin6_port = hostToNetwork16(port);
    } else {
        memZero(&addr_, sizeof(addr_));
        addr_.sin_family = AF_INET;
        in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
        addr_.sin_addr.s_addr = hostToNetwork32(ip);
        addr_.sin_port = hostToNetwork16(port);
    }
}

InetAddress::InetAddress(StringArg ip, uint16_t port, bool ipv6)
{
    if (ipv6) {
        memZero(&addr6_, sizeof(addr6_));
        sockets::fromIpPort(ip.c_str(), port, &addr6_);
    } else {
        memZero(&addr_, sizeof(addr_));
        sockets::fromIpPort(ip.c_str(), port, &addr_);
    }
}

string InetAddress::toIpPort() const
{
    char buf[64] = "";
    sockets::toIpPort(buf, sizeof(buf), getSockAddr());
    return buf;
}

string InetAddress::toIp() const
{
    char buf[64] = "";
    sockets::toIp(buf, sizeof(buf), getSockAddr());
    return buf;
}

uint32_t InetAddress::ipNetEndian() const
{
    assert(family() == AF_INET);
    return addr_.sin_addr.s_addr;
}

uint16_t InetAddress::toPort() const
{
    return networkToHost16(portNetEndian());
}

static __thread char t_resolveBuffer[64 * 1024];

bool InetAddress::resolve(StringArg hostname, InetAddress* out)
{
    assert(out != NULL);
    struct hostent hent;
    struct hostent* he = NULL;
    int herrno = 0; 
    memZero(&hent, sizeof(hent));

    int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof(t_resolveBuffer), &he, &herrno);

    if (ret == 0 && he != NULL) {
        assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
        out->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
        return true;
    } else {
        if (ret) {
            printf("InetAddress::resolve error\n");
        }
        return false;
    }
}

void InetAddress::setScopeId(uint32_t scope_id)
{
    if (family() == AF_INET6) {
        addr6_.sin6_scope_id = scope_id;
    }
}