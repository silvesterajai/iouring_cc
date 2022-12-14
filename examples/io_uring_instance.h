#pragma once

#include "iouring/io_uring.h"

namespace iouring
{
class IoUringInst
{
public:
    static constexpr io_buf_gid_t gid = 8193;
public:
    static IoUring &Get()
    {
        IoUringParams params;
        params.AddProvidedBuffer(gid, 1024, 10)
              .ShouldSupportOpProvideBuffers()
              .ShouldSupportFeatFastPoll();
        static IoUring _inst(params);
        return _inst;
    }
private:
    IoUringInst() = default;
    ~IoUringInst() = default;

}; // class IoUringInst
} // namespace iouring