/*
 * Copyright 2020 ScyllaDB
 */

#pragma once

#include <cinttypes>
#include <cstdint>
#include <linux/openat2.h>
#include <memory>
#include <stdio.h>
#include <string>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "iouring/io_completion.h"
#include "iouring/io_uring_utils.h"

namespace iouring {

// Forward declaration
class IoUring;

struct IoApiCommonVars
{
    IoApiCommonVars()
        : sqe_flag_(0)
        , op_flag_(0)
        , multishot_(false)
        , buf_gid_(IOURING_INVALID_BUFF_GID)
        , file_index_(IOURING_INVALID_FILEINDEX)
        , direct_alloc_(false)
    {}

    IoApiCommonVars(flag_t sqe_flag, flag_t op_flag)
        : sqe_flag_(sqe_flag)
        , op_flag_(op_flag)
        , multishot_(false)
        , buf_gid_(IOURING_INVALID_BUFF_GID)
        , file_index_(IOURING_INVALID_FILEINDEX)
        , direct_alloc_(false)
    {}

    IoApiCommonVars &SqeFlag(flag_t sqe_flag)
    {
        sqe_flag_ = sqe_flag;
        return *this;
    }

    IoApiCommonVars &OpFlag(flag_t op_flag)
    {
        op_flag_ = op_flag;
        return *this;
    }

    IoApiCommonVars &Multishot()
    {
        multishot_ = true;
        return *this;
    }

    IoApiCommonVars &Gid(const io_buf_gid_t &gid)
    {
        buf_gid_ = gid;
        return *this;
    }

    IoApiCommonVars &FileIndex(const int32_t &file_index)
    {
        file_index_ = file_index;
        return *this;
    }

    IoApiCommonVars &Ioprio(const uint16_t ioprio)
    {
        ioprio_ = ioprio;
        return *this;
    }

    IoApiCommonVars &DirectAlloc()
    {
        direct_alloc_ = true;
        return *this;
    }

    // params
    flag_t sqe_flag_;
    flag_t op_flag_;
    bool multishot_;
    uint32_t buf_gid_;
    int32_t file_index_;
    int16_t ioprio_;
    bool direct_alloc_;
};

class IoRequest
{
    friend class IoUring;

public:
    using Operation = iouring::Operation;

public:
    explicit IoRequest(Operation op)
        : operation_(op)
    {}

    static std::string OperationStr(const Operation &op)
    {
        switch (op)
        {
        case Operation::IORING_OP_NOP:
            return "NOP";
        case Operation::IORING_OP_READV:
            return "READV";
        case Operation::IORING_OP_WRITEV:
            return "WRITEV";
        case Operation::IORING_OP_FSYNC:
            return "FSYNC";
        case Operation::IORING_OP_READ_FIXED:
            return "READFIXED";
        case Operation::IORING_OP_WRITE_FIXED:
            return "WRITEFIXED";
        case Operation::IORING_OP_POLL_ADD:
            return "POLL_ADD";
        case Operation::IORING_OP_POLL_REMOVE:
            return "POLL_REMOVE";
        case Operation::IORING_OP_SYNC_FILE_RANGE:
            return "IORING_OP_SYNC_FILE_RANGE";
        case Operation::IORING_OP_SENDMSG:
            return "SENDMSG";
        case Operation::IORING_OP_RECVMSG:
            return "RECVMSG";
        case Operation::IORING_OP_TIMEOUT:
            return "TIMEOUT";
        case Operation::IORING_OP_TIMEOUT_REMOVE:
            return "IORING_OP_TIMEOUT_REMOVE";
        case Operation::IORING_OP_ACCEPT:
            return "ACCEPT";
        case Operation::IORING_OP_ASYNC_CANCEL:
            return "CANCEL";
        case Operation::IORING_OP_LINK_TIMEOUT:
            return "IORING_OP_LINK_TIMEOUT";
        case Operation::IORING_OP_CONNECT:
            return "CONNECT";
        case Operation::IORING_OP_FALLOCATE:
            return "FALLOCATE";
        case Operation::IORING_OP_OPENAT:
            return "OPENAT";
        case Operation::IORING_OP_CLOSE:
            return "CLOSE";
        case Operation::IORING_OP_FILES_UPDATE:
            return "IORING_OP_FILES_UPDATE";
        case Operation::IORING_OP_STATX:
            return "STATX";
        case Operation::IORING_OP_READ:
            return "READ";
        case Operation::IORING_OP_WRITE:
            return "WRITE";
        case Operation::IORING_OP_FADVISE:
            return "IORING_OP_FADVISE";
        case Operation::IORING_OP_MADVISE:
            return "IORING_OP_MADVISE";
        case Operation::IORING_OP_SEND:
            return "SEND";
        case Operation::IORING_OP_RECV:
            return "RECV";
        case Operation::IORING_OP_OPENAT2:
            return "IORING_OP_OPENAT2";
        case Operation::IORING_OP_EPOLL_CTL:
            return "EPOLL_CTL";
        case Operation::IORING_OP_SPLICE:
            return "SPLICE";
        case Operation::IORING_OP_PROVIDE_BUFFERS:
            return "PROVIDE_BUFFERS";
        case Operation::IORING_OP_REMOVE_BUFFERS:
            return "REMOVE_BUFFERS";
        case Operation::IORING_OP_TEE:
            return "TEE";
        case Operation::IORING_OP_SHUTDOWN:
            return "SHUTDOWN";
        case Operation::IORING_OP_RENAMEAT:
            return "RENAMEAT";
        case Operation::IORING_OP_UNLINKAT:
            return "UNLINKAT";
        case Operation::IORING_OP_MKDIRAT:
            return "MAKEDIRAT";
        case Operation::IORING_OP_SYMLINKAT:
            return "SYMLINKAT";
        case Operation::IORING_OP_LINKAT:
            return "LINKAT";
        case Operation::IORING_OP_MSG_RING:
            return "IORING_OP_MSG_RING";
        case Operation::IORING_OP_FSETXATTR:
            return "IORING_OP_FSETXATTR";
        case Operation::IORING_OP_SETXATTR:
            return "IORING_OP_SETXATTR";
        case Operation::IORING_OP_FGETXATTR:
            return "IORING_OP_FGETXATTR";
        case Operation::IORING_OP_GETXATTR:
            return "IORING_OP_GETXATTR";
        case Operation::IORING_OP_SOCKET:
            return "IORING_OP_SOCKET";
        case Operation::IORING_OP_URING_CMD:
            return "IORING_OP_URING_CMD";
        case Operation::IORING_OP_SEND_ZC:
            return "IORING_OP_SEND_ZC";
        case Operation::IORING_OP_SENDMSG_ZC:
            return "IORING_OP_SENDMSG_ZC";
        default:
            return "UNKNOWN";
        }
        return "UNKNOWN";
    }

private:
    struct NoOp
    {};

    struct ReadvOp
    {
        fd_t fd;
        offset_t offset;
        iovec *iov;
        io_size_t iov_len;
        flag_t rw_flags;
    };

    struct WritevOp
    {
        fd_t fd;
        offset_t offset;
        const iovec *iov;
        io_size_t iov_len;
        flag_t rw_flags;
    };

    struct FsyncOp
    {
        fd_t fd;
        flag_t flags;
    };

    struct ReadFixedOp
    {
        fd_t fd;
        offset_t offset;
        char_t *addr;
        io_size_t size;
        index_t index;
    };

    struct WriteFixedOp
    {
        fd_t fd;
        offset_t offset;
        const char_t *addr;
        io_size_t size;
        index_t index;
    };

    struct PollAddOp
    {
        fd_t fd;
        flag_t events;
        bool multishot;
    };

    struct PollRemoveOp
    {
        pointer_t user_data;
        pointer_t new_user_data;
        flag_t events;
        flag_t flags;
    };

    struct SyncFileRangeOp
    {
        fd_t fd;
        io_size_t len;
        offset_t offset;
        flag_t flags;
    };

    struct SendmsgOp
    {
        fd_t fd;
        msghdr_t *msghdr;
        flag_t flags;
    };

    struct RecvmsgOp
    {
        fd_t fd;
        msghdr_t *msghdr;
        flag_t flags;
        bool multishot;
        uint32_t buf_gid;
    };

    struct TimeoutOp
    {
        std::chrono::nanoseconds timeout;
        uint32_t count;
        flag_t flags;
    };

    struct TimeoutRemoveOp
    {
        std::chrono::nanoseconds timeout;
        pointer_t user_data;
        flag_t flags;
    };

    struct AcceptOp
    {
        fd_t fd;
        socket_addr_t *sockaddr;
        socket_len_t *socklen;
        flag_t flags;
        int32_t file_index;
        bool multishot;
    };

    struct CancelOp
    {
        void *user_data;
        uint64_t user_data_64;
        fd_t fd;
        flag_t flags;
    };

    struct LinkTimeoutOp
    {
        std::chrono::nanoseconds timeout;
        flag_t flags;
    };

    struct ConnectOp
    {
        fd_t fd;
        const socket_addr_t *sockaddr;
        socket_len_t socklen;
    };

    struct FallocateOp
    {
        fd_t fd;
        flag_t mode;
        offset_t offset;
        io_size_t len;
    };

    struct OpenatOp
    {
        fd_t dirfd;
        const char *path_name;
        flag_t flags;
        flag_t mode;
        int32_t file_index;
    };

    struct CloseOp
    {
        fd_t fd;
        int32_t file_index;
    };

    struct FilesUpdateOp
    {
        fd_t *fds;
        io_size_t fds_len;
        uint32_t offset;
    };

    struct ReadOp
    {
        fd_t fd;
        offset_t offset;
        char_t *addr;
        io_size_t size;
        uint32_t buf_gid;
    };

    struct WriteOp
    {
        fd_t fd;
        offset_t offset;
        const char_t *addr;
        io_size_t size;
    };

    struct FadviseOp
    {
        fd_t fd;
        offset_t offset;
        io_size_t len;
        int advice;
    };

    struct MadviseOp
    {
        void *addr;
        io_size_t len;
        int advice;
    };

    struct SendOp
    {
        fd_t fd;
        const char_t *addr;
        io_size_t size;
        flag_t flags;
        flag_t zc_flags;
        int32_t file_index;
    };

    struct RecvOp
    {
        fd_t fd;
        char_t *addr;
        io_size_t size;
        flag_t flags;
        uint32_t buf_gid;
    };

    struct Openat2Op
    {
        fd_t dirfd;
        const char *path_name;
        flag_t flags;
        struct open_how *how;
        int32_t file_index;
    };

    struct EpollCtlOp
    {
        fd_t epfd;
        fd_t fd;
        int op;
        struct epoll_event *event;
    };

    struct SpliceOp
    {
        fd_t in_fd;
        offset_t in_offset;
        fd_t out_fd;
        offset_t out_offset;
        io_size_t size;
        flag_t flags;
    };

    struct ProvideBuffOp
    {
        char_t *buffer;
        io_size_t len;
        uint32_t nbufs;
        int32_t gid;
        uint16_t bid;
    };

    struct TeeOp
    {
        fd_t in_fd;
        fd_t out_fd;
        io_size_t size;
        flag_t flags;
    };

    struct StatxOp
    {
        fd_t dirfd;
        const char *path_name;
        flag_t flags;
        flag_t mask;
        struct statx *statxbuf;
    };

    struct ShutdownOp
    {
        fd_t fd;
        flag_t how;
    };

    struct RenameatOp
    {
        fd_t olddirfd;
        const char *oldpath;
        fd_t newdirfd;
        const char *newpath;
        flag_t flags;
    };

    struct UnlinkatOp
    {
        fd_t dirfd;
        const char *path;
        flag_t flags;
    };

    struct MakediratOp
    {
        fd_t dirfd;
        const char *path;
        flag_t mode;
    };

    struct SymlinkatOp
    {
        const char *target;
        fd_t newdirfd;
        const char *linkpath;
    };

    struct LinkatOp
    {
        fd_t olddirfd;
        const char *oldpath;
        fd_t newdirfd;
        const char *newpath;
        flag_t flags;
    };

    struct MsgringOp
    {
        fd_t fd;
        uint32_t data_32;
        uint64_t data_64;
        flag_t flags;
    };

    struct XattrOp
    {
        fd_t fd;
        const char *path;
        const char *name;
        const char *value;
        flag_t flags;
        io_size_t len;
    };

    struct SocketOp
    {
        int domain;
        int type;
        int protocol;
        flag_t flags;
        int32_t file_index;
        bool direct_alloc;
    };

    struct Flags
    {
        flag_t sqe_flag;
        flag_t op_flag;
    };

public:
    static std::unique_ptr<IoRequest> make_noop(const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_NOP);
        // clang-format off
        req->noop_ = {
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_readv(fd_t fd, offset_t offset, std::vector<iovec> &iovecs, flag_t rw_flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_READV);
        // clang-format off
        req->readv_  = {
            .fd = fd,
            .offset = offset,
            .iov = iovecs.data(),
            .iov_len = iovecs.size(),
            .rw_flags = rw_flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_writev(fd_t fd, offset_t offset, std::vector<iovec> &iovecs, flag_t rw_flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_WRITEV);
        // clang-format off
        req->writev_  = {
            .fd = fd,
            .offset = offset,
            .iov = iovecs.data(),
            .iov_len = iovecs.size(),
            .rw_flags = rw_flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_fsync(fd_t fd, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_FSYNC);
        // clang-format off
        req->fsync_ = {
            .fd = fd,
            .flags = flags
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_read_fixed(fd_t fd, offset_t offset, void *address, io_size_t size, index_t buf_index, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_READ_FIXED);
        // clang-format off
        req->read_fixed_ = {
            .fd = fd,
            .offset = offset,
            .addr = reinterpret_cast<char_t*>(address),
            .size = size,
            .index = buf_index
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_write_fixed(fd_t fd, offset_t offset, const void *address, io_size_t size, index_t buf_index, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_WRITE_FIXED);
        // clang-format off
        req->write_fixed_ = {
            .fd = fd,
            .offset = offset,
            .addr = static_cast<const char_t*>(address),
            .size = size,
            .index = buf_index
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_poll_add(fd_t fd, flag_t events, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_POLL_ADD);
        // clang-format off
        req->poll_add_ = {
            .fd = fd,
            .events = events,
            .multishot = common_vars.multishot_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_poll_remove(void *user_data, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_POLL_REMOVE);
        // clang-format off
        req->poll_remove_ = {
            .user_data = reinterpret_cast<pointer_t>(user_data),
            .new_user_data = 0,
            .events = 0,
            .flags = 0,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_poll_update(void *old_user_data, void *new_user_data, flag_t events, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_POLL_REMOVE);
        // clang-format off
        req->poll_remove_ = {
            .user_data = reinterpret_cast<pointer_t>(old_user_data),
            .new_user_data = reinterpret_cast<pointer_t>(new_user_data),
            .events = events,
            .flags = flags
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_sync_filerange(fd_t fd, io_size_t len, offset_t offset, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SYNC_FILE_RANGE);
        // clang-format off
        req->sync_filerange_ = {
            .fd = fd,
            .len = len,
            .offset = offset,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_sendmsg(fd_t fd, msghdr_t *msghdr, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SENDMSG);
        // clang-format off
        req->sendmsg_ = {
            .fd = fd,
            .msghdr = msghdr,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_recvmsg(fd_t fd, msghdr_t *msghdr, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_RECVMSG);
        // clang-format off
        req->recvmsg_ = {
            .fd = fd,
            .msghdr = msghdr,
            .flags = flags,
            .multishot = common_vars.multishot_,
            .buf_gid = common_vars.buf_gid_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_timeout(std::chrono::nanoseconds &timeout, const uint32_t &count, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_TIMEOUT);
        // clang-format off
        req->timeout_ = {
            .timeout = timeout,
            .count = count,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_timeout_remove(pointer_t user_data, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_TIMEOUT_REMOVE);
        // clang-format off
        req->timeout_remove_ = {
            .timeout = std::chrono::nanoseconds(0),
            .user_data = user_data,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_timeout_update(std::chrono::nanoseconds &timeout, pointer_t user_data, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_TIMEOUT_REMOVE);
        // clang-format off
        req->timeout_remove_ = {
            .timeout = timeout,
            .user_data = user_data,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_accept(fd_t fd, socket_addr_t *addr, socket_len_t *len, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_ACCEPT);
        // clang-format off
        req->accept_ = {
            .fd = fd,
            .sockaddr = addr,
            .socklen = len,
            .flags = flags,
            .file_index = common_vars.file_index_,
            .multishot = common_vars.multishot_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        if (common_vars.file_index_ != IOURING_INVALID_FILEINDEX)
        {
            req->flags_.sqe_flag |= IOSQE_FIXED_FILE;
        }
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_cancel(void *user_data, uint64_t user_data_64, fd_t fd, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_ASYNC_CANCEL);
        // clang-format off
        req->cancel_ = {
            .user_data = user_data,
            .user_data_64 = user_data_64,
            .fd = fd,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_link_timeout(std::chrono::nanoseconds &timeout, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_LINK_TIMEOUT);
        // clang-format off
        req->link_timeout_ = {
            .timeout = timeout,
            .flags = flags
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_connect(fd_t fd, const socket_addr_t *addr, socket_len_t len, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_CONNECT);
        // clang-format off
        req->connect_ = {
            .fd = fd,
            .sockaddr = addr,
            .socklen = len
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_fallocate(fd_t fd, flag_t mode, offset_t offset, io_size_t len, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_FALLOCATE);
        // clang-format off
        req->fallocate_ = {
            .fd = fd,
            .mode = mode,
            .offset = offset,
            .len = len
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_openat(fd_t dirfd, const char *pathname, flag_t flags, flag_t mode, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_OPENAT);
        // clang-format off
        req->openat_ = {
            .dirfd = dirfd,
            .path_name = pathname,
            .flags = flags,
            .mode = mode,
            .file_index = common_vars.file_index_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        if (common_vars.file_index_ != IOURING_INVALID_FILEINDEX)
        {
            req->flags_.sqe_flag |= IOSQE_FIXED_FILE;
        }
        return req;
    }

    static std::unique_ptr<IoRequest> make_close(fd_t fd, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_CLOSE);
        // clang-format off
        req->close_ = {
            .fd = fd,
            .file_index = common_vars.file_index_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        if (common_vars.file_index_ != IOURING_INVALID_FILEINDEX)
        {
            req->flags_.sqe_flag |= IOSQE_FIXED_FILE;
        }
        return req;
    }

    static std::unique_ptr<IoRequest> make_files_update(std::vector<fd_t> &fds, uint32_t offset, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_FILES_UPDATE);
        // clang-format off
        req->files_update_ = {
            .fds = fds.data(),
            .fds_len = fds.size(),
            .offset = offset,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_statx(fd_t dirfd, const char *pathname, flag_t flags, flag_t mask, struct statx *statxbuf, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_STATX);
        // clang-format off
        req->statx_ = {
            .dirfd = dirfd,
            .path_name = pathname,
            .flags = flags,
            .mask = mask,
            .statxbuf = statxbuf,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_read(fd_t fd, offset_t offset, void *address, io_size_t size, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_READ);
        // clang-format off
        req->read_ = {
            .fd = fd,
            .offset = offset,
            .addr = reinterpret_cast<char_t*>(address),
            .size = size,
            .buf_gid = common_vars.buf_gid_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        if (common_vars.buf_gid_ != IOURING_INVALID_BUFF_GID)
        {
            req->flags_.sqe_flag |= IOSQE_BUFFER_SELECT;
        }
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_write(fd_t fd, offset_t offset, const void *address, io_size_t size, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_WRITE);
        // clang-format off
        req->write_ = {
            .fd = fd,
            .offset = offset,
            .addr = static_cast<const char_t*>(address),
            .size = size
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_fadvise(fd_t fd, offset_t offset, io_size_t len, int advice, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_FADVISE);
        // clang-format off
        req->fadvise_  = {
            .fd = fd,
            .offset = offset,
            .len = len,
            .advice = advice,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_madvise(void *addr, io_size_t len, int advice, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_MADVISE);
        // clang-format off
        req->madvise_  = {
            .addr = addr,
            .len = len,
            .advice = advice,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_send(fd_t fd, const void *address, io_size_t size, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SEND);
        // clang-format off
        req->send_ = {
            .fd = fd,
            .addr = static_cast<const char_t*>(address),
            .size = size,
            .flags = flags,
            .zc_flags = 0,
            .file_index = common_vars.file_index_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_recv(fd_t fd, void *address, io_size_t size, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_RECV);
        // clang-format off
        req->recv_ = {
            .fd = fd,
            .addr = reinterpret_cast<char_t*>(address),
            .size = size,
            .flags = flags,
            .buf_gid = common_vars.buf_gid_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        // The multishot version requires length to be 0 , the IOSQE_BUFFER_SELECT flag to be set and no MSG_WAITALL flag to be set.
        if (common_vars.buf_gid_ != IOURING_INVALID_BUFF_GID)
        {
            req->flags_.sqe_flag |= IOSQE_BUFFER_SELECT;
        }
        if (common_vars.multishot_)
        {
            req->recv_.flags |= MSG_WAITALL;
        }
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_openat2(fd_t dirfd, const char *pathname, flag_t flags, struct open_how *how, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_OPENAT2);
        // clang-format off
        req->openat2_ = {
            .dirfd = dirfd,
            .path_name = pathname,
            .flags = flags,
            .how = how,
            .file_index = common_vars.file_index_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        if (common_vars.file_index_ != IOURING_INVALID_FILEINDEX)
        {
            req->flags_.sqe_flag |= IOSQE_FIXED_FILE;
        }
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_epoll_ctl(fd_t epfd, fd_t fd, int op, struct epoll_event *event, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_EPOLL_CTL);
        // clang-format off
        req->epoll_ctl_ = {
            .epfd = epfd,
            .fd = fd,
            .op = op,
            .event = event,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_splice(fd_t in_fd,
                                                  offset_t in_offset,
                                                  fd_t out_fd,
                                                  offset_t out_offset,
                                                  io_size_t size,
                                                  flag_t flags,
                                                  const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SPLICE);
        // clang-format off
        req->splice_ = {
            .in_fd = in_fd,
            .in_offset = in_offset,
            .out_fd = out_fd,
            .out_offset = out_offset,
            .size = size,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_provide_buffer(char_t *buffer,
                                                          io_size_t len,
                                                          uint32_t nbufs,
                                                          const io_buf_gid_t &gid,
                                                          const io_buf_bid_t &bid,
                                                          const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_PROVIDE_BUFFERS);
        // clang-format off
        req->provide_buff_ = {
            .buffer = buffer,
            .len = len,
            .nbufs = nbufs,
            .gid = gid,
            .bid = bid,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_remove_buffer(uint32_t nbufs, io_buf_gid_t gid, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_REMOVE_BUFFERS);
        // clang-format off
        req->provide_buff_ = {
            .buffer = nullptr,
            .len = 0,
            .nbufs = nbufs,
            .gid = gid,
            .bid = 0,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_tee(fd_t in_fd, fd_t out_fd, io_size_t size, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_TEE);
        // clang-format off
        req->tee_ = {
            .in_fd = in_fd,
            .out_fd = out_fd,
            .size = size,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_shutdown(fd_t fd, flag_t how, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SHUTDOWN);
        // clang-format off
        req->shutdown_ = {
            .fd = fd,
            .how = how,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_renameat(fd_t olddirfd, const char *oldpath, fd_t newdirfd, const char *newpath, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_RENAMEAT);
        // clang-format off
        req->renameat_ = {
            .olddirfd = olddirfd,
            .oldpath = oldpath,
            .newdirfd = newdirfd,
            .newpath = newpath,
            .flags = flags
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_unlinkat(fd_t dirfd, const char *path, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_UNLINKAT);
        // clang-format off
        req->unlinkat_ = {
            .dirfd = dirfd,
            .path = path,
            .flags = flags
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_makedirat(fd_t dirfd, const char *path, flag_t mode, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_MKDIRAT);
        // clang-format off
        req->makedirat_ = {
            .dirfd = dirfd,
            .path = path,
            .mode = mode,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_symlinkat(const char *target, fd_t newdirfd, const char *linkpath, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SYMLINKAT);
        // clang-format off
        req->symlinkat_ = {
            .target = target,
            .newdirfd = newdirfd,
            .linkpath = linkpath,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_linkat(fd_t olddirfd, const char *oldpath, fd_t newdirfd, const char *newpath, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_LINKAT);
        // clang-format off
        req->linkat_ = {
            .olddirfd = olddirfd,
            .oldpath = oldpath,
            .newdirfd = newdirfd,
            .newpath = newpath,
            .flags = flags
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_msgring(fd_t fd, const uint32_t &data_32, const uint64_t &data_64, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_MSG_RING);
        // clang-format off
        req->msgring_ = {
            .fd = fd,
            .data_32 = data_32,
            .data_64 = data_64,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_fsetxattr(fd_t fd, const char *name, const char *value, flag_t flags, io_size_t len, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_FSETXATTR);
        // clang-format off
        req->xattr_ = {
            .fd = fd,
            .path = nullptr,
            .name = name,
            .value = value,
            .flags = flags,
            .len = len,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_setxattr(const char *path, const char *name, const char *value, flag_t flags, io_size_t len, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SETXATTR);
        // clang-format off
        req->xattr_ = {
            .fd = -1,
            .path = path,
            .name = name,
            .value = value,
            .flags = flags,
            .len = len,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_fgetxattr(fd_t fd, const char *name, const char *value, io_size_t len, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_FGETXATTR);
        // clang-format off
        req->xattr_ = {
            .fd = fd,
            .path = nullptr,
            .name = name,
            .value = value,
            .flags = 0,
            .len = len,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_getxattr(const char *path, const char *name, const char *value, io_size_t len, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_FGETXATTR);
        // clang-format off
        req->xattr_ = {
            .fd = -1,
            .path = path,
            .name = name,
            .value = value,
            .flags = 0,
            .len = len,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_socket(int domain, int type, int protocol, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_FGETXATTR);
        // clang-format off
        req->socket_ = {
            .domain = domain,
            .type = type,
            .protocol = protocol,
            .flags = flags,
            .file_index = common_vars.file_index_,
            .direct_alloc = common_vars.direct_alloc_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest>
    make_sendzc(fd_t fd, const void *address, io_size_t size, flag_t flags, flag_t zc_flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SEND_ZC);
        // clang-format off
        req->send_ = {
            .fd = fd,
            .addr = static_cast<const char_t*>(address),
            .size = size,
            .flags = flags,
            .zc_flags = zc_flags,
            .file_index = common_vars.file_index_,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    static std::unique_ptr<IoRequest> make_sendmsgzc(fd_t fd, msghdr_t *msghdr, flag_t flags, const IoApiCommonVars &common_vars)
    {
        auto req = std::make_unique<IoRequest>(Operation::IORING_OP_SENDMSG_ZC);
        // clang-format off
        req->sendmsg_ = {
            .fd = fd,
            .msghdr = msghdr,
            .flags = flags,
        };
        req->flags_ = {
            .sqe_flag = common_vars.sqe_flag_,
            .op_flag = common_vars.op_flag_,
        };
        // clang-format on
        return req;
    }

    bool IsRead() const
    {
        switch (operation_)
        {
        case Operation::IORING_OP_READ:
        case Operation::IORING_OP_READV:
        case Operation::IORING_OP_RECVMSG:
        case Operation::IORING_OP_RECV:
        case Operation::IORING_OP_READ_FIXED:
            return true;
        default:
            return false;
        }
    }

    bool IsWrite() const
    {
        switch (operation_)
        {
        case Operation::IORING_OP_WRITE:
        case Operation::IORING_OP_WRITEV:
        case Operation::IORING_OP_SENDMSG:
        case Operation::IORING_OP_SEND:
        case Operation::IORING_OP_SEND_ZC:
        case Operation::IORING_OP_SENDMSG_ZC:
        case Operation::IORING_OP_WRITE_FIXED:
            return true;
        default:
            return false;
        }
    }

    Operation OpCode() const
    {
        return operation_;
    }

    template<Operation Op>
    auto &As() const
    {
        if constexpr (Op == Operation::IORING_OP_NOP)
        {
            return noop_;
        }
        if constexpr (Op == Operation::IORING_OP_READ)
        {
            return read_;
        }
        if constexpr (Op == Operation::IORING_OP_READV)
        {
            return readv_;
        }
        if constexpr (Op == Operation::IORING_OP_RECV)
        {
            return recv_;
        }
        if constexpr (Op == Operation::IORING_OP_RECVMSG)
        {
            return recvmsg_;
        }
        if constexpr (Op == Operation::IORING_OP_SEND)
        {
            return send_;
        }
        if constexpr (Op == Operation::IORING_OP_SENDMSG)
        {
            return sendmsg_;
        }
        if constexpr (Op == Operation::IORING_OP_WRITE)
        {
            return write_;
        }
        if constexpr (Op == Operation::IORING_OP_WRITEV)
        {
            return writev_;
        }
        if constexpr (Op == Operation::IORING_OP_ACCEPT)
        {
            return accept_;
        }
        if constexpr (Op == Operation::IORING_OP_CONNECT)
        {
            return connect_;
        }
        if constexpr (Op == Operation::IORING_OP_POLL_ADD)
        {
            return poll_add_;
        }
        if constexpr (Op == Operation::IORING_OP_POLL_REMOVE)
        {
            return poll_remove_;
        }
        if constexpr (Op == Operation::IORING_OP_ASYNC_CANCEL)
        {
            return cancel_;
        }
        if constexpr (Op == Operation::IORING_OP_FALLOCATE)
        {
            return fallocate_;
        }
        if constexpr (Op == Operation::IORING_OP_CLOSE)
        {
            return close_;
        }
        if constexpr (Op == Operation::IORING_OP_FSYNC)
        {
            return fsync_;
        }
        if constexpr (Op == Operation::IORING_OP_READ_FIXED)
        {
            return read_fixed_;
        }
        if constexpr (Op == Operation::IORING_OP_WRITE_FIXED)
        {
            return write_fixed_;
        }
        if constexpr (Op == Operation::IORING_OP_TIMEOUT)
        {
            return timeout_;
        }
        if constexpr (Op == Operation::IORING_OP_LINK_TIMEOUT)
        {
            return link_timeout_;
        }
        if constexpr (Op == Operation::IORING_OP_OPENAT)
        {
            return openat_;
        }
        if constexpr (Op == Operation::IORING_OP_STATX)
        {
            return statx_;
        }
        if constexpr (Op == Operation::IORING_OP_SPLICE)
        {
            return splice_;
        }
        if constexpr (Op == Operation::IORING_OP_TEE)
        {
            return tee_;
        }
        if constexpr (Op == Operation::IORING_OP_SHUTDOWN)
        {
            return shutdown_;
        }
        if constexpr (Op == Operation::IORING_OP_RENAMEAT)
        {
            return renameat_;
        }
        if constexpr (Op == Operation::IORING_OP_SYMLINKAT)
        {
            return symlinkat_;
        }
        if constexpr (Op == Operation::IORING_OP_LINKAT)
        {
            return linkat_;
        }
        if constexpr (Op == Operation::IORING_OP_UNLINKAT)
        {
            return unlinkat_;
        }
        if constexpr (Op == Operation::IORING_OP_MKDIRAT)
        {
            return makedirat_;
        }
        if constexpr (Op == Operation::IORING_OP_EPOLL_CTL)
        {
            return epoll_ctl_;
        }
        if constexpr (Op == Operation::IORING_OP_PROVIDE_BUFFERS)
        {
            return provide_buff_;
        }
        if constexpr (Op == Operation::IORING_OP_REMOVE_BUFFERS)
        {
            return provide_buff_;
        }
        if constexpr (Op == Operation::IORING_OP_SYNC_FILE_RANGE)
        {
            return sync_filerange_;
        }
        if constexpr (Op == Operation::IORING_OP_TIMEOUT_REMOVE)
        {
            return timeout_remove_;
        }
        if constexpr (Op == Operation::IORING_OP_FILES_UPDATE)
        {
            return files_update_;
        }
        if constexpr (Op == Operation::IORING_OP_FADVISE)
        {
            return fadvise_;
        }
        if constexpr (Op == Operation::IORING_OP_MADVISE)
        {
            return madvise_;
        }
        if constexpr (Op == Operation::IORING_OP_OPENAT2)
        {
            return openat2_;
        }
        if constexpr (Op == Operation::IORING_OP_MSG_RING)
        {
            return msgring_;
        }
        if constexpr (Op == Operation::IORING_OP_FSETXATTR)
        {
            return xattr_;
        }
        if constexpr (Op == Operation::IORING_OP_SETXATTR)
        {
            return xattr_;
        }
        if constexpr (Op == Operation::IORING_OP_FGETXATTR)
        {
            return xattr_;
        }
        if constexpr (Op == Operation::IORING_OP_GETXATTR)
        {
            return xattr_;
        }
        if constexpr (Op == Operation::IORING_OP_SOCKET)
        {
            return socket_;
        }
        if constexpr (Op == Operation::IORING_OP_SEND_ZC)
        {
            return send_;
        }
        if constexpr (Op == Operation::IORING_OP_SENDMSG_ZC)
        {
            return sendmsg_;
        }
    }

private:
    Operation operation_;

    union
    {
        NoOp noop_;
        ReadOp read_;
        ReadvOp readv_;
        RecvOp recv_;
        RecvmsgOp recvmsg_;
        SendOp send_;
        SendmsgOp sendmsg_;
        WriteOp write_;
        WritevOp writev_;
        AcceptOp accept_;
        ConnectOp connect_;
        PollAddOp poll_add_;
        PollRemoveOp poll_remove_;
        CancelOp cancel_;
        FallocateOp fallocate_;
        CloseOp close_;
        FsyncOp fsync_;
        ReadFixedOp read_fixed_;
        WriteFixedOp write_fixed_;
        TimeoutOp timeout_;
        TimeoutRemoveOp timeout_remove_;
        LinkTimeoutOp link_timeout_;
        OpenatOp openat_;
        StatxOp statx_;
        SpliceOp splice_;
        TeeOp tee_;
        ShutdownOp shutdown_;
        RenameatOp renameat_;
        SymlinkatOp symlinkat_;
        LinkatOp linkat_;
        UnlinkatOp unlinkat_;
        MakediratOp makedirat_;
        EpollCtlOp epoll_ctl_;
        ProvideBuffOp provide_buff_;
        SyncFileRangeOp sync_filerange_;
        FilesUpdateOp files_update_;
        FadviseOp fadvise_;
        MadviseOp madvise_;
        Openat2Op openat2_;
        MsgringOp msgring_;
        XattrOp xattr_;
        SocketOp socket_;
    };

    Flags flags_ = {0, 0};

}; // class IoRequest
} // namespace iouring
