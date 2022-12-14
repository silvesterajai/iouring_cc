

#include <sys/epoll.h>
#include "examples/io_uring_instance.h"
#include "iouring/io_uring_api.h"

namespace iouring {
namespace net {
namespace ops {

int EpollCtl(int epoll_fd, int op, int register_fd, struct epoll_event *event)
{
#ifndef USE_IOURING
    return epoll_ctl(epoll_fd, op, register_fd, event);
#else
    auto &io_ring = IoUringInst::Get();
    auto ret_future = iouring::EpollCtl(io_ring, epoll_fd, register_fd, op, event);
    auto ret = ret_future.Get();
    return ret;
#endif
}

} // namesapce ops
} // namespace net
} // namesapce iouring