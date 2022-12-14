#include "examples/netio/acceptor.h"
#include "socket_ops.h"
#include "iouring/logger.h"

using namespace iouring;
using namespace iouring::net;

Acceptor::Acceptor(const InetAddress &listen_addr, bool reuse_port)
    : accept_socket_(Socket::CreateTcp(listen_addr.Family()))
    , listening_(false)
{

    accept_socket_.SetReuseAddr(true);
    accept_socket_.SetReusePort(reuse_port);
    accept_socket_.BindAddress(listen_addr);
    Listen();
}

void Acceptor::Listen()
{
    if (listening_)
        return;
    listening_ = true;
    accept_socket_.Listen();
}

void Acceptor::EpollCb(int32_t fd, [[maybe_unused]]uint32_t events)
{
    if (fd != accept_socket_.Fd())
    {
        LOG_ERROR("Listening fd {} does to match with received {}", accept_socket_.Fd(), fd);
        return;
    }
    Accept();
}

void Acceptor::Accept()
{
    IOURING_ASSERT(listening_, "Acceptor::Not listening");
    InetAddress peer_addr;
    int connfd = accept_socket_.Accept(&peer_addr);
    if (connfd >= 0)
    {
        LOG_DEBUG("Received new connection {}", connfd);
        if (new_connection_cb_)
        {
            LOG_DEBUG("Invoking new connection callback");
            new_connection_cb_(connfd, peer_addr);
        }
        else
        {
            ops::Close(connfd);
        }
    }
}