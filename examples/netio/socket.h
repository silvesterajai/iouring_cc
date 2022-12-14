#pragma once
#include "iouring/assert.h"
#include "iouring/noncopyable.h"
#include "iouring/io_uring_utils.h"
#include <arpa/inet.h>
#include <memory>
#include <sys/types.h>
#include <vector>

namespace iouring {
namespace net {

class InetAddress;

class Socket : public NonCopyable
{
public:
    explicit Socket(int sockfd)
        : sockfd_(sockfd)
    {
        IOURING_ASSERT(sockfd_ >= 0, "");
    }

    ~Socket();

    Socket &operator=(Socket &&other) noexcept
    {
        IOURING_ASSERT(this != std::addressof(other), "Cannot move same socket");
        sockfd_ = other.sockfd_;
        other.sockfd_ = -1;
        return *this;
    }

    void Swap(Socket &other) noexcept
    {
        const int tmp = sockfd_;
        sockfd_ = other.sockfd_;
        other.sockfd_ = tmp;
    }

    int Fd() const
    {
        return sockfd_;
    }

    void BindAddress(const InetAddress &local_addr);
    void Listen();
    void Close();
    int Connect(const InetAddress &server_addr);
    int Accept(InetAddress *peeraddr);
    int Send(const char *buf, size_t count, int flags=0);
    int Recv(char *buf, size_t count, int flags=0);
    const BufferInfo Recv(const size_t &count, int flags=0);

    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    void SetTcpNoDelay(bool on);

    /// Enable/disable SO_REUSEADDR
    void SetReuseAddr(bool on);

    /// Enable/disable SO_REUSEPORT
    void SetReusePort(bool on);

    /// Enable/disable SO_KEEPALIVE
    void SetKeepAlive(bool on);

private:
    int sockfd_;

public:
    static Socket CreateTcp(sa_family_t family); // AF_INET or AF_INET6
    static Socket CreateUdp(sa_family_t family); // AF_INET or AF_INET6

}; // class Socket

// class SocketStream
// {
// public:
//     SocketStream(Socket &socket) : socket_(socket)
//     {
//     }
//     ~SocketStream() = default;
//     ssize_t Read(char *buff, size_t size, int flags=0)
//     {
//         return socket_.Recv(buff, size, flags);
//     }
//     int Fd()
//     {
//         return socket_.Fd();
//     }
//     ssize_t Write(const char *buff, size_t size, int flags=0)
//     {
//         return socket_.Send(buff, size, flags);
//     }
// private:
//     Socket &socket_;
// };
} // namespace net
} // namespace iouring