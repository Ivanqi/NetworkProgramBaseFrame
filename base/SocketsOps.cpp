#include "SocketsOps.h"
#include "Types.h"
#include "Endian.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>      // snprintf
#include <sys/uio.h>    // readv
#include <unistd.h>
#include <assert.h>

void setNonBlockAndCloseOnExec(int sockfd)
{
    // non-block
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);

    // close-on-exec
    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);

    (void) ret;
}

// sockaddr_in6转换为sockaddr，给操作系统使用
const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in6* addr)
{
    return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

// sockaddr_in6转换为sockaddr，给操作系统使用
struct sockaddr* sockets::sockaddr_cast(struct sockaddr_in6* addr)
{
    return static_cast<struct sockaddr*>(implicit_cast<void*>(addr));
}

// sockaddr_in转换为sockaddr，给操作系统使用
const struct sockaddr* sockets::sockaddr_cast(const struct sockaddr_in* addr)
{
    return static_cast<const struct sockaddr*>(implicit_cast<const void*>(addr));
}

// sockaddr转换为sockaddr_in
const struct sockaddr_in* sockets::sockaddr_in_cast(const struct sockaddr* addr)
{
    return static_cast<const struct sockaddr_in*>(implicit_cast<const void*>(addr));
}

// sockaddr转换为sockaddr_in6
const struct sockaddr_in6* sockets::sockaddr_in6_cast(const struct sockaddr* addr)
{
  return static_cast<const struct sockaddr_in6*>(implicit_cast<const void*>(addr));
}

int sockets::createNonblockingOrDie(sa_family_t family)
{
    /**
     * SOCK_STREAM
     *  提供一个顺序化的，可靠的，全双工的，基于连接的字节流。
     *  支持数据传送流量控制机制。TCP协议即基于这种流式套接字
     * 
     * SOCK_NONBLOCK
     *  在新的打开文件描述上设置O_NONBLOCK status标志
     *  使用此标志可保存对fcntl(2)的额外调用以实现相同的效果结果 
     * 
     * SOCK_CLOEXEC
     *  在新的文件描述符上设置close on exec（FD_CLOEXEC）标志
     *  有关原因，请参阅open（2）中对O_CLOEXEC标志的描述
     *  调用open函数O_CLOEXEC模式打开的文件描述符在执行exec调用新程序中关闭，且为原子操作
     *  调用open函数不使用O_CLOEXEC模式打开的文件描述符，然后调用fcntl 函数设置FD_CLOEXEC选项，效果和使用O_CLOEXEC选项open函数相同
     * 
     * IPPROTO_TCP
     *  Tcp 协议
     */
    int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    assert(sockfd > 0);
    return sockfd;
}

void sockets::bindOrDie(int sockfd, const struct sockaddr* addr)
{
    int ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
    assert(ret != -1);
}

void sockets::listenOrDie(int sockfd)
{
    int ret = ::listen(sockfd, SOMAXCONN);
    assert(ret != -1);
}

int sockets::accept(int sockfd, struct sockaddr_in6* addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof(*addr));
    int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
    setNonBlockAndCloseOnExec(connfd);


    if (connfd < 0) {
        int savedErrno = errno;
        printf("accept4 error\n");
        switch (savedErrno) {
            case EAGAIN:
            case ECONNABORTED:
            case EINTR:
            case EPROTO:
            case EPERM:
            case EMFILE:
                errno = savedErrno;
                break;
            case EBADF:
            case EFAULT:
            case EINVAL:
            case ENFILE:
            case ENOBUFS:
            case ENOMEM:
            case ENOTSOCK:
            case EOPNOTSUPP:
                printf("unexpected error of ::accept :%d", savedErrno);
                break;
            default:
                printf("unknown error of ::accept %d", savedErrno);
                break;
        }
    }
    
    return connfd;
}

int sockets::connect(int sockfd, const struct sockaddr* addr)
{
    return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
}

ssize_t sockets::read(int sockfd, void *buf, size_t count)
{
  return ::read(sockfd, buf, count);
}

ssize_t sockets::readv(int sockfd, const struct iovec *iov, int iovcnt)
{
  return ::readv(sockfd, iov, iovcnt);
}

ssize_t sockets::write(int sockfd, const void *buf, size_t count)
{
  return ::write(sockfd, buf, count);
}

void sockets::close(int sockfd)
{
    /**
     * int close(int fd);
     * 当使用完文件后若已不再需要则可使用 close()关闭该文件
     *  close()会让数据写回磁盘, 并释放该文件所占用的资源
     * 参数fd 为先前由open()或creat()所返回的文件描述词.
     * 
     * 返回值
     *  若文件顺利关闭则返回0
     *  发生错误时返回-1
     */
    assert(::close(sockfd) == 0);
}

void sockets::shutdownWrite(int sockfd)
{
    /**
     * int shutdown(int s, int how);
     * shutdown()用来终止参数s 所指定的socket 连线. 参数s 是连线中的socket 处理代码
     * 参数how有下列几种情况
     *  how=0 终止读取操作.
     *  how=1 终止传送操作
     *  how=2 终止读取及传送操作
     * 
     * 返回值
     *  成功则返回0
     *  失败返回-1
     */
    assert(::shutdown(sockfd, SHUT_WR) == 0);
}

void sockets::toIpPort(char *buf, size_t size, const struct sockaddr* addr)
{
    toIp(buf, size, addr);
    size_t end = ::strlen(buf);
    const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
    uint16_t port = networkToHost16(addr4->sin_port);
    assert(size > end);
    snprintf(buf + end, size - end, ":%u", port);
}

void sockets::toIp(char *buf, size_t size, const struct sockaddr* addr)
{
    if (addr->sa_family == AF_INET) {
        assert(size >= INET_ADDRSTRLEN);
        const struct sockaddr_in* addr4 = sockaddr_in_cast(addr);
        ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(size));
    } else if (addr->sa_family == AF_INET6) {
        assert(size >= INET6_ADDRSTRLEN);
        const struct sockaddr_in6* addr6 = sockaddr_in6_cast(addr);
        ::inet_ntop(AF_INET6, &addr6->sin6_addr, buf, static_cast<socklen_t>(size));
    }
}

void sockets::fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = hostToNetwork16(port);
    /**
     * int inet_pton(int af, const char *src, void *dst);
     * inet_pton（）在成功时返回1（网络地址成功已转换）
     * 如果src不包含字符串，则返回0表示指定地址族中的有效网络地址
     * 如果af不包含有效的地址族，则返回-1并errno设置为EAFNOSUPPORT。
     */
    int ret = ::inet_pton(AF_INET, ip, &addr->sin_addr);
    assert(ret == 1);
}

void sockets::fromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr)
{
    addr->sin6_family = AF_INET6;
    addr->sin6_port = hostToNetwork16(port);
    assert (::inet_pton(AF_INET6, ip, &addr->sin6_addr) == 1);
}

int sockets::getSocketError(int sockfd)
{
    int optval;
    socklen_t optlen = static_cast<socklen_t>(sizeof(optval));

    // getsockopt 获取socket状态
    // 成功时，返回0。出错时，返回-1，错误原因存于errno中。
    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        return errno;
    } else {
        return optval;
    }
}

struct sockaddr_in6 sockets::getLocalAddr(int sockfd)
{
    struct sockaddr_in6 localaddr;
    memZero(&localaddr, sizeof localaddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof(localaddr));

    // getsockname（）返回套接字sockfd的当前地址。绑定在addr指向的缓冲区中
    // 成功时，返回0。发生错误时，返回-1，错误原因存于errno中
    assert (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) != -1);
    return localaddr;
}

struct sockaddr_in6 sockets::getPeerAddr(int sockfd)
{
    struct sockaddr_in6 peeraddr;
    memZero(&peeraddr, sizeof peeraddr);
    socklen_t addrlen = static_cast<socklen_t>(sizeof peeraddr);

    // getpeername() 返回连接到套接字的对端的地址 sockfd，绑定在addr指向的缓冲区中。相当于客户端地址
    // 成功时，返回0。发生错误时，返回-1，错误原因存于errno中。
    assert(::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) == 0);
    return peeraddr;
}

bool sockets::isSelfConnect(int sockfd)
{
    struct sockaddr_in6 localaddr = getLocalAddr(sockfd);
    struct sockaddr_in6 peeraddr = getPeerAddr(sockfd);

    if (localaddr.sin6_family == AF_INET) {
        const struct sockaddr_in* laddr4 = reinterpret_cast<struct sockaddr_in*>(&localaddr);
        const struct sockaddr_in* raddr4 = reinterpret_cast<struct sockaddr_in*>(&peeraddr);

        return laddr4->sin_port == raddr4->sin_port && laddr4->sin_addr.s_addr == raddr4->sin_addr.s_addr;

    } else if (localaddr.sin6_family == AF_INET6) {
        return localaddr.sin6_port == peeraddr.sin6_port 
            && memcmp(&localaddr.sin6_addr, &peeraddr.sin6_addr, sizeof localaddr.sin6_addr) == 0;

    } else {
        return false;
    }
}