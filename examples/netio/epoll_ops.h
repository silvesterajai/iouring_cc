#pragma once

#include "iouring/noncopyable.h"
#include "iouring/status.h"
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <sys/epoll.h>
#include <unordered_set>

namespace iouring {
namespace net {
namespace ops {

int EpollCtl(int epoll_fd, int op, int register_fd, struct epoll_event *event);

} // namesapce ops
} // namespace net
} // namesapce iouring