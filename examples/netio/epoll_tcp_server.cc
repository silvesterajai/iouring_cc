#include "examples/netio/acceptor.h"
#include "examples/netio/epoll.h"
#include "iouring/logger.h"
#include "iouring/buffer.h"
#include <map>
#include <memory>

using namespace iouring::net;
using namespace iouring;
using namespace std::placeholders;

class ClientContext;

static std::map<int32_t, std::unique_ptr<ClientContext>> listeners_;
static Epoll epoll_;

class ClientContext
{
public:
    ClientContext(int sockfd, const InetAddress &peer_address)
        : socket_(sockfd)
        , recv_buffer(1024)
    {
            if(peer_address.Family() == AF_INET)
            {
                peer_address_.SetSockAddrInet(peer_address.AsV4());
            }
            else
            {
                peer_address_.SetSockAddrInet6(peer_address.AsV6());
            }
    }
    ~ClientContext()
    {

    }

    void Handle(const uint32_t &events)
    {
        if (events & (EPOLLIN | EPOLLPRI))
        {
            recv_buffer.clear();
            // auto ret = socket_.Recv(recv_buffer.data(), recv_buffer.capacity());
            auto ret_buff_info = socket_.Recv(2048);
            LOG_DEBUG("Received message from client of length {}", ret_buff_info.size_);
            if (ret_buff_info.size_ > 0)
            {
                LOG_DEBUG("Sending message to client of length {}. {}", ret_buff_info.size_, std::string((char*)ret_buff_info.buffer_,ret_buff_info.size_));
                socket_.Send(reinterpret_cast<const char*>(ret_buff_info.buffer_), ret_buff_info.size_);
            }
            ops::ProvideBuffer(ret_buff_info.buffer_,  ret_buff_info.buf_gid_, ret_buff_info.buf_bid_);
        }
    }

private:
    Socket socket_;
    InetAddress peer_address_;
    iouring::Buffer<char> recv_buffer;
}; // class ClientContext

void OnEvent(int sockfd, uint32_t events)
{
    auto it = listeners_.find(sockfd);
    if (it == listeners_.end())
    {
        auto ret = epoll_.UnregisterHandler(it->first);
        return;
    }
    it->second->Handle(events);
    // error
    if (events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP))
    {
        auto ret = epoll_.UnregisterHandler(it->first);
        listeners_.erase(it);
    }
}

void OnConnect(int sockfd, const InetAddress &peer_address)
{
    auto connected_sock = std::make_unique<ClientContext>(sockfd, peer_address);
    auto status = epoll_.RegisterHandler(sockfd, std::bind(OnEvent, _1, _2), EPOLLIN | EPOLLPRI);
    if (!status.Ok())
        return;
    listeners_.emplace(sockfd, std::move(connected_sock));
}
 

int main()
{
    spdlog::set_level(spdlog::level::debug);
    auto status = epoll_.Open();
    if (!status.Ok())
    {
        LOG_ERROR("Failed to open epoll, {}", status.ErrorMessage());
        return (int)status.Code();
    }
    const int port = 6530;
    InetAddress server_addr(port);
    Acceptor acceptor(server_addr);
    status = epoll_.RegisterHandler(acceptor.ListenFd(), std::bind(&Acceptor::EpollCb, &acceptor, _1, _2), EPOLLIN | EPOLLPRI );
    if (!status.Ok())
    {
        LOG_ERROR("Failed to register acceptor fd to epoll");
        return (int)status.Code();
    }
    acceptor.SetNewConnectionCallback(OnConnect);
    while (true)
    {
        int32_t events = 0;
        status = epoll_.Wait(std::nullopt, events);
        if (!status.Ok())
        {
            LOG_ERROR("Failed on epoll wait");
            return (int)status.Code();
        }
        LOG_DEBUG("Processed {} number of events", events);
    }
    LOG_WARN("Exiting tcp_server");
}
