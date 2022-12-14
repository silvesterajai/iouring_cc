#pragma once

#include "iouring/noncopyable.h"
#include "iouring/status.h"
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <sys/epoll.h>
#include <atomic>
#include <sys/types.h>
#include <unordered_set>

namespace iouring {
namespace net {
class Epoll : public NonCopyable
{
public:
    using Handler = std::function<void(int32_t fd, uint32_t events)>;

public:
    Epoll();
    ~Epoll();
    Status Open();
    Status RegisterHandler(int fd, Handler handler, uint32_t events);
    Status UnregisterHandler(int fd);
    Status Wait(std::optional<std::chrono::milliseconds> timeout, int32_t &num_events);

private:
    void Stop(uint32_t events);
    ssize_t CloseFd(int &fd);
    void HandlerError(int &fd);
private:
    struct Info
    {
        uint32_t events;
        Handler handler;
    };

private:
    int epoll_fd_;
    int event_fd_; // eventfd for Epoll
    std::map<int, Info> epoll_handlers_;
    std::unordered_set<int> to_remove_;
    std::atomic<bool> stop_;

}; // class Epoll
} // namespace net
} // namespace iouring