#include "examples/netio/socket_ops.h"
#include "examples/io_uring_instance.h"
#include "io_completion.h"
#include "iouring/assert.h"
#include "iouring/io_uring_api.h"
#include "iouring/logger.h"
#include <asm-generic/errno.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <liburing/io_uring.h>
#include <sys/uio.h>
#include <unistd.h>

namespace iouring {
namespace net {
namespace ops {
const struct sockaddr *sockaddr_cast(const struct sockaddr_in6 *addr)
{
    return static_cast<const struct sockaddr *>(static_cast<const void *>(addr));
}

struct sockaddr *sockaddr_cast(struct sockaddr_in6 *addr)
{
    return static_cast<struct sockaddr *>(static_cast<void *>(addr));
}

const struct sockaddr *sockaddr_cast(const struct sockaddr_in *addr)
{
    return static_cast<const struct sockaddr *>(static_cast<const void *>(addr));
}

struct sockaddr *sockaddr_cast(struct sockaddr_in *addr)
{
    return static_cast<struct sockaddr *>(static_cast<void *>(addr));
}

const struct sockaddr_in *sockaddr_in_cast(const struct sockaddr *addr)
{
    return static_cast<const struct sockaddr_in *>(static_cast<const void *>(addr));
}

const struct sockaddr_in6 *sockaddr_in6_cast(const struct sockaddr *addr)
{
    return static_cast<const struct sockaddr_in6 *>(static_cast<const void *>(addr));
}

int CreateSocket(sa_family_t family, int type, int protocol)
{
    int sockfd = ::socket(family, type, protocol);
    IOURING_ASSERT(sockfd > 0, "net::CreateSocket");
    return sockfd;
}

void Bind(int sockfd, const struct sockaddr *addr)
{
    auto len = static_cast<socklen_t>(sizeof(struct sockaddr_in6));
    [[maybe_unused]]int ret = ::bind(sockfd, addr, len);
    IOURING_ASSERT(ret >= 0, "net::Bind");
}

int Connect(int sockfd, const struct sockaddr *addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof *addr);
#ifndef USE_IOURING
    return ::connect(sockfd, addr, addrlen);
#else
    auto timeout = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(2));
    auto &io_ring = IoUringInst::Get();
    auto ret_future = iouring::Connect(io_ring, sockfd, addr, addrlen, NoOpIoCompletionCb, IoApiCommonVars().SqeFlag(IOSQE_IO_LINK));
    auto timeout_future = iouring::LinkTimeout(io_ring, timeout);
    auto ret = ret_future.Get();
    return ret;
#endif
}

void Listen(int sockfd)
{
    [[maybe_unused]]int ret = ::listen(sockfd, SOMAXCONN);
    IOURING_ASSERT(ret >= 0, "net::Listen");
}

int Accept(int sockfd, struct sockaddr *addr)
{
    socklen_t addrlen = static_cast<socklen_t>(sizeof *addr);
#ifndef USE_IOURING
    int connfd = ::accept4(sockfd, addr, &addrlen, SOCK_CLOEXEC);
    LOG_ERROR("Errno {}, {}", errno, strerror(errno));
    return connfd;
#else
    auto &io_ring = IoUringInst::Get();
    auto ret_future = iouring::Accept(io_ring, sockfd, addr, &addrlen, 0);
    auto ret = ret_future.Get();
    return ret;
#endif
}

ssize_t Read(int sockfd, void *buf, size_t count)
{
#ifndef USE_IOURING
    return ::read(sockfd, buf, count);
#else
    auto &io_ring = IoUringInst::Get();
    auto ret_future = iouring::Read(io_ring, sockfd, buf, count, -1);
    auto ret = ret_future.Get();
    return ret;
#endif
}

ssize_t Readv(int sockfd, const struct iovec *iov, int iovcnt)
{
    return ::readv(sockfd, iov, iovcnt);
}

ssize_t Write(int sockfd, const void *buf, size_t count)
{
#ifndef USE_IOURING
    return ::write(sockfd, buf, count);
#else
    auto &io_ring = IoUringInst::Get();
    auto ret_future = iouring::Write(io_ring, sockfd, buf, count, -1);
    auto ret = ret_future.Get();
    return ret;
#endif
}

ssize_t Close(int sockfd)
{
    return ::close(sockfd);
}

ssize_t Send(int sockfd, const void *buf, size_t count, int flags)
{
#ifndef USE_IOURING
    return ::send(sockfd, buf, count, flags);
#else
    auto &io_ring = IoUringInst::Get();
    auto ret_future = iouring::Send(io_ring, sockfd, buf, count, flags);
    auto ret = ret_future.Get();
    return ret;
#endif
}

ssize_t Recv(int sockfd, void *buf, size_t count, int flags)
{
#ifndef USE_IOURING
    return ::recv(sockfd, buf, count, flags);
#else
    auto &io_ring = IoUringInst::Get();
    auto ret_future = iouring::Recv(io_ring, sockfd, buf, count, flags);
    auto ret = ret_future.Get();
    return ret.size_;
#endif
}

const BufferInfo Recv(int sockfd, const size_t &count, int flags)
{
#ifndef USE_IOURING
    IOURING_ASSERT(false, "not implemented");
#else
    auto &io_ring = IoUringInst::Get();
    while (true)
    {
        auto timeout = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(2));
        auto recv_future = iouring::Recv(
            io_ring, sockfd, nullptr, count, flags, NoOpIoCompletionCb, IoApiCommonVars().Gid(IoUringInst::gid).SqeFlag(IOSQE_IO_LINK));
        auto timeout_future = iouring::LinkTimeout(io_ring, timeout);
        auto timeout_res = timeout_future.Get();
        if (timeout_res == -ETIME) // connect timeout
        {
            auto cancel_future = iouring::Cancel(io_ring, recv_future.CancelToken(), 0);
            [[maybe_unused]] auto cancel_res = cancel_future.Get();
        }
        else if (timeout_res == -ECANCELED)
        {
            return recv_future.Get();
        }
        else
        {
            IOURING_ASSERT(false, fmt::format("Unexpected timeout res {}", timeout_res));
        }
    }
    IOURING_ASSERT(true, "Unreachable code");
#endif
}

void ProvideBuffer(char_t *buffer, const io_buf_gid_t &gid, const io_buf_bid_t &bid)
{
    auto &io_ring = IoUringInst::Get();
    auto ret_future = iouring::ProvideBuffer(io_ring, buffer, 0, 1, gid, bid);
    ret_future.Get();
}

} // namespace ops
} // namespace net
} // namespace iouring