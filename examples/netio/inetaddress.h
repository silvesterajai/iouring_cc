#pragma once
#include "examples/netio/socket_ops.h"
#include "iouring/noncopyable.h"
#include <netinet/in.h>
#include <string>

namespace iouring {
namespace net {

class InetAddress : public NonCopyable
{

public:
    explicit InetAddress(uint16_t port = 0, bool loopback = false, bool ipv6 = false);
    InetAddress(std::string ip, uint16_t port, bool ipv6 = false);

    explicit InetAddress(const struct sockaddr_in &addr)
        : addr_(addr)
    {}

    explicit InetAddress(const struct sockaddr_in6 &addr)
        : addr6_(addr)
    {}

    sa_family_t Family() const
    {
        return addr_.sin_family;
    }

    uint16_t Port() const noexcept
    {
        return ntohs(addr_.sin_port);
    }

    socklen_t Length() const noexcept
    {
        return Family() == AF_INET6 ? sizeof(addr6_) : sizeof(addr_);
    }

    const struct sockaddr *GetSockAddr()
    {
        return Family() == AF_INET6 ? ops::sockaddr_cast(&addr6_) : ops::sockaddr_cast(&addr_);
    }

    const struct sockaddr *GetSockAddr() const
    {
        return Family() == AF_INET6 ? ops::sockaddr_cast(&addr6_) : ops::sockaddr_cast(&addr_);
    }

    void SetSockAddrInet(const struct sockaddr_in &addr)
    {
        addr_ = addr;
    }

    void SetSockAddrInet6(const struct sockaddr_in6 &addr6)
    {
        addr6_ = addr6;
    }

    const struct sockaddr_in &AsV4() const
    {
        return addr_;
    }

    const struct sockaddr_in6 &AsV6() const
    {
        return addr6_;
    }

    void SetScopeId(uint32_t scope_id);

private:
    union
    {
        struct sockaddr_in addr_;
        struct sockaddr_in6 addr6_;
    };
}; // class InetAddress
} // namespace net
} // namespace iouring