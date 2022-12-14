#include "examples/netio/socket.h"
#include "examples/netio/inetaddress.h"
#include "examples/netio/socket_ops.h"
#include <netinet/in.h>
#include <netinet/tcp.h>

using namespace iouring;
using namespace iouring::net;

Socket::~Socket()
{
    Close();
}

void Socket::BindAddress(const InetAddress &localaddr)
{
    ops::Bind(sockfd_, localaddr.GetSockAddr());
}

void Socket::Close()
{
    if (sockfd_ != -1)
        ops::Close(sockfd_);
    sockfd_ = -1;
}

void Socket::Listen()
{
    ops::Listen(sockfd_);
}

int Socket::Connect(const InetAddress &server_addr)
{
    return ops::Connect(sockfd_, server_addr.GetSockAddr());
}

int Socket::Accept(InetAddress *peeraddr)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    int connfd = ops::Accept(sockfd_, ops::sockaddr_cast(&addr));
    if (connfd >= 0)
    {
        peeraddr->SetSockAddrInet(addr);
    }
    return connfd;
}

int Socket::Send(const char *buf, size_t count, int flags)
{
    return ops::Send(sockfd_, buf, count, flags);
}

int Socket::Recv(char *buf, size_t count, int flags)
{
    return ops::Recv(sockfd_, buf, count, flags);
}

const BufferInfo Socket::Recv(const size_t &count, int flags)
{
    return ops::Recv(sockfd_, count, flags);
}

void Socket::SetTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::SetReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::SetReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof optval));
}

void Socket::SetKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof optval));
}

Socket Socket::CreateTcp(sa_family_t family)
{
    const int sockfd = ops::CreateSocket(family, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    IOURING_ASSERT(sockfd >= 0, "");
    return Socket{sockfd};
}

Socket Socket::CreateUdp(sa_family_t family)
{
    const int sockfd = ops::CreateSocket(family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
    IOURING_ASSERT(sockfd >= 0, "");
    return Socket{sockfd};
}