#include "examples/netio/epoll.h"
#include "examples/netio/epoll_ops.h"
#include "iouring/logger.h"
#include "iouring/defer.h"
#include "examples/netio/socket_ops.h"
#include <fcntl.h>
#include <limits.h>
#include <functional>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

using namespace iouring;
using namespace iouring::net;
using namespace std::placeholders;

Epoll::Epoll()
    : epoll_fd_(-1)
    , event_fd_(-1)
    , stop_(false)
{}

Epoll::~Epoll()
{
    CloseFd(epoll_fd_);
    CloseFd(event_fd_);
    stop_ = true;
}

ssize_t Epoll::CloseFd(int &fd)
{
    if (fd < 0)
        return 0;
    DeferredAction set([&fd]{fd = -1;});
    return ops::Close(fd);
}

Status Epoll::Open()
{
    if (epoll_fd_ >= 0)
        return Status::OkStatus();
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0)
        return Status::InternalError("Failed to create epoll fd");
    event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd_ < 0)
        return Status::InternalError("Failed to create event fd");
    auto ret = RegisterHandler(event_fd_, std::bind(&Epoll::Stop, this, _1), EPOLLIN | EPOLLPRI);
    if (!ret.Ok())
        return ret;
    return Status::OkStatus();
}

void Epoll::Stop([[maybe_unused]]uint32_t events)
{
    LOG_WARN("Epoll stop event received");
    uint64_t val = 0;
    ops::Read(event_fd_, (char*)&val, sizeof(val));
    stop_ = true;
    [[maybe_unused]]auto ret = UnregisterHandler(event_fd_);
}

Status Epoll::RegisterHandler(int fd, Handler handler, uint32_t events)
{
    if (fd < 0)
    {
        return Status::InvalidArgumentError("Invalid file descriptor");
    }
    if (!events)
    {
        return Status::InvalidArgumentError("No events are specified. Atlease one event should be specified");
    }
    events |= EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    auto [it, inserted] = epoll_handlers_.emplace(fd, Info{
                                                          .events = events,
                                                          .handler = std::move(handler),
                                                      });
    int EPOLL_OP = EPOLL_CTL_ADD;
    if (!inserted)
    {
        EPOLL_OP = EPOLL_CTL_MOD;
        epoll_handlers_.erase(it);
        [[maybe_unused]] auto [it, inserted] = epoll_handlers_.emplace(fd, Info{
                                                                               .events = events,
                                                                               .handler = std::move(handler),
                                                                           });
    }
    epoll_event ev= {
        .events = events,
        .data = {
            .fd=fd
        },
    };

    if (ops::EpollCtl(epoll_fd_, EPOLL_OP, fd, &ev) == -1)
    {
        epoll_handlers_.erase(fd);
        return Status::InternalError("Failed to add fd via epoll_ctl");
    }
    return Status::OkStatus();
}

Status Epoll::UnregisterHandler(int fd)
{
    if (ops::EpollCtl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
        return Status::InternalError("Failed to remove fd via epoll_ctl");
    }
    auto it = epoll_handlers_.find(fd);
    if (it == epoll_handlers_.end())
    {
        return Status::InternalError("FD is not registered");
    }
    to_remove_.insert(it->first);
    return Status::OkStatus();
}

Status Epoll::Wait(std::optional<std::chrono::milliseconds> timeout, int32_t &num_events)
{
    if (stop_)
    {
        num_events = -1;
        return Status::UnavailableError("Epoll is not running or already stopped");
    }
    num_events = 0;
    int timeout_ms = -1;
    if (timeout && timeout->count() < INT_MAX)
    {
        timeout_ms = timeout->count();
    }
    const auto max_events = epoll_handlers_.size();
    epoll_event ev[max_events];
    num_events = epoll_wait(epoll_fd_, ev, max_events, timeout_ms);
    if (num_events < -1)
    {
        return Status::UnknownError("Failed epoll_wait");
    }
    for (int i = 0; i < num_events; ++i)
    {
        const auto it = epoll_handlers_.find(ev[i].data.fd);
        if (it == epoll_handlers_.end())
        {
            LOG_ERROR("Received event for fd {}, without handler. Might be marked for removal.", (int32_t)ev[i].data.fd);
            continue;
        }
        const Info &info = it->second;
        if ((info.events & (EPOLLIN | EPOLLPRI)) == (EPOLLIN | EPOLLPRI) && (ev[i].events & EPOLLIN) != ev[i].events)
        {
            LOG_ERROR("Invalid event received. Event: {0:#x}", (int32_t)ev[i].events);
        }
        // handle events
        info.handler(ev[i].data.fd, ev[i].events);
        // oneshot
        if (info.events & EPOLLONESHOT)
        {
            auto remove_ret = UnregisterHandler(ev[i].data.fd);
        }
        // error
        if (ev[i].events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP))
        {
            // info.handler should handle the specific error.
        }

    }
    for (auto fd : to_remove_)
    {
        epoll_handlers_.erase(fd);
    }
    to_remove_.clear();
    return Status::OkStatus();
}

void Epoll::HandlerError(int &fd)
{
    if (fd < 0)
        return;
    auto ret = UnregisterHandler(fd);
}