#include "iouring/logger.h"
#include "iouring/buffer.h"
#include "examples/netio/socket.h"
#include "examples/netio/inetaddress.h"

using namespace iouring::net;

class Client
{
public:
    Client(const InetAddress &server_address)
        : socket_(Socket::CreateTcp(server_address.Family()))
        , recv_buffer(1024)
    {
            if(server_address.Family() == AF_INET)
            {
                server_address_.SetSockAddrInet(server_address.AsV4());
            }
            else
            {
                server_address_.SetSockAddrInet6(server_address.AsV6());
            }
    }
    ~Client()
    {

    }

    void Connect()
    {
        auto ret = socket_.Connect(server_address_);
        if (ret >= 0)
        {
            LOG_DEBUG("Connected to server");
            Run();
        }
    }
    void Run()
    {
        static const std::string message = "Hello World!";
        recv_buffer.clear();
        recv_buffer.append(message.c_str(), message.length());
        auto ret = socket_.Send(recv_buffer.data(), recv_buffer.size());
        if (ret > 0)
        {
            LOG_DEBUG("Send message to server of length {}", ret);
            // ret = socket_.Recv(recv_buffer.data(), recv_buffer.capacity());
            auto ret_buff_info = socket_.Recv(2048);
            LOG_DEBUG("Received message from client of length {}", ret_buff_info.size_);
            if (ret_buff_info.size_ > 0)
            {
                LOG_DEBUG("Received message from server of length {}. {}", ret_buff_info.size_, std::string(ret_buff_info.buffer_,ret_buff_info.size_));
            }
            ops::ProvideBuffer(ret_buff_info.buffer_,  ret_buff_info.buf_gid_, ret_buff_info.buf_bid_);
        }
    }

private:
    Socket socket_;
    InetAddress server_address_;
    iouring::Buffer<char> recv_buffer;
}; // class Client

int main()
{
    spdlog::set_level(spdlog::level::debug);
    const int port = 6530;
    InetAddress server_addr(port);
    auto connector = Socket::CreateTcp(server_addr.Family());
    connector.SetReuseAddr(true);
    connector.SetReusePort(true);
    Client client(server_addr);
    client.Connect();
    LOG_WARN("Exiting tcp_client");
}