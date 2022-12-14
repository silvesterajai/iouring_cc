#pragma once

#include "examples/netio/inetaddress.h"
#include "examples/netio/socket.h"
#include "iouring/noncopyable.h"
#include <functional>

namespace iouring {
namespace net {
class Acceptor : public NonCopyable
{
public:
    typedef std::function<void(int sockfd, const InetAddress &)> NewConnectionCallback;

public:
    Acceptor(const InetAddress &listenAddr, bool reuse_port = true);
    ~Acceptor() = default;

    void SetNewConnectionCallback(const NewConnectionCallback &cb)
    {
        new_connection_cb_ = cb;
    }

    void Listen();

    bool Listening() const
    {
        return listening_;
    }
    int ListenFd() const noexcept { return accept_socket_.Fd(); }
    void EpollCb(int32_t fd, uint32_t events);
    void Accept();

private:
    Socket accept_socket_;
    bool listening_;
    NewConnectionCallback new_connection_cb_;
}; // class Acceptor
} // namespace net
} // namespace iouring