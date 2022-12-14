#include "examples/netio/acceptor.h"
#include "examples/netio/epoll.h"
#include "io_uring_api.h"
#include "iouring/logger.h"
#include "iouring/buffer.h"
#include "iouring/threadpool.h"

using namespace iouring::net;
ThreadPool thread_pool(3);

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

    void Run()
    {
        recv_buffer.clear();
        // auto ret = socket_.Recv(recv_buffer.data(), recv_buffer.capacity());
        auto ret_buff_info = socket_.Recv(2048);
        LOG_DEBUG("Received message from client of length {}", ret_buff_info.size_);
        if (ret_buff_info.size_ > 0)
        {
            LOG_DEBUG("Sending message to client of length {}", ret_buff_info.size_);
            socket_.Send(reinterpret_cast<const char*>(ret_buff_info.buffer_), ret_buff_info.size_);
        }
        ops::ProvideBuffer(ret_buff_info.buffer_, ret_buff_info.buf_gid_, ret_buff_info.buf_bid_);
    }

private:
    Socket socket_;
    InetAddress peer_address_;
    iouring::Buffer<char> recv_buffer;
}; // class ClientContext

void OnConnect(int sockfd, const InetAddress &peer_address)
{
    thread_pool.enqueue([sockfd, &peer_address]{
        ClientContext client_ctx(sockfd, peer_address);
        client_ctx.Run();
    });
}

class Server
{
public:
    Server() = default;
    void Start()
    {
        auto ret = epoll_.Open();
        if (!ret.Ok())
            IOURING_ASSERT(false, "Failed epoll");
    }
    ~Server();
private:
    Epoll epoll_;
    
}; // class Server

int main()
{
    spdlog::set_level(spdlog::level::debug);
    const int port = 6530;
    InetAddress server_addr(port);
    Acceptor acceptor(server_addr);
    acceptor.Listen();
    acceptor.SetNewConnectionCallback(OnConnect);
    while(true)
    {
        acceptor.Accept();
    }
    LOG_WARN("Exiting tcp_server");
}
