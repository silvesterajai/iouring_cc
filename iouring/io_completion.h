/*
 * Copyright 2020 ScyllaDB
 */

#pragma once
#include <cinttypes>
#include <cstdint>
#include <functional>
#include <future>
#include <stddef.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "iouring/buffer.h"
#include "iouring/io_uring_utils.h"
#include "liburing/io_uring.h"

namespace iouring {

class IoCompletion;
using IoCompletionCb = std::function<void(ssize_t res, IoCompletion *)>;

template<typename T>
class IoFuture
{
public:
    explicit IoFuture(std::future<T> &future, void *cancel_token)
        : future_(future)
        , cancel_token_(cancel_token)
    {}

    explicit IoFuture(std::future<T> &&future, void *cancel_token)
        : future_(std::forward<std::future<T>>(future))
        , cancel_token_(cancel_token)
    {}

    IoFuture(IoFuture &&other)
        : future_(std::move(other.future_))
        , cancel_token_(std::exchange(other.cancel_token_, nullptr))
    {}

    T Get()
    {
        return future_.get();
    }

    void *CancelToken()
    {
        return cancel_token_;
    }

private:
    std::future<T> future_;
    void *cancel_token_;
};

constexpr void NoOpIoCompletionCb([[maybe_unused]] ssize_t res, [[maybe_unused]] IoCompletion *comp)
{
    // No op
}

/// Base class of user_data to be stored in SQE and CQE
class IoCompletion
{

public:
    virtual ~IoCompletion() = default;

    /// Will be invoked by CQE processing context.
    /// `res` is response received in `cqe->res`
    virtual void CompleteWith(const cqe_t *cqe, char_t *provided_buffer = nullptr) final
    {
        cqe_res_ = cqe->res;
        cqe_flags_ = cqe->flags;
        if (buf_gid_ != IOURING_INVALID_BUFF_GID)
        {
            if (IOURING_LIKELY(cqe->flags & IORING_CQE_F_BUFFER))
            {
                // IORING_CQE_F_BUFFER	If set, the upper 16 bits are the buffer ID
                buf_bid_ = cqe_flags_ >> 16;
            }
        }
        Complete(cqe, provided_buffer);
    }

    virtual void RegisterCallback(IoCompletionCb callback)
    {
        callback_ = callback;
    }

    /// To be implemented in the derived class
    virtual void Complete(const cqe_t *cqe, char_t *provided_buffer = nullptr) noexcept = 0;
    /// If there is any exception while processing SQE/CQE, it will stored to be processed by the callee context.
    virtual void SetException(std::exception_ptr eptr) noexcept = 0;

    virtual const flag_t &Res()
    {
        return cqe_res_;
    }

    virtual const int32_t &Gid()
    {
        return buf_gid_;
    }

    virtual const uint16_t &Bid()
    {
        return buf_bid_;
    }

    virtual const flag_t &Flags()
    {
        return cqe_flags_;
    }

    virtual bool More()
    {
        return (cqe_flags_ & IORING_CQE_F_MORE);
    }

protected:
    virtual void InvokeCallback(const ssize_t &res)
    {
        if (callback_)
        {
            callback_(res, this);
        }
    }

protected:
    IoCompletionCb callback_;
    flag_t cqe_res_;
    flag_t cqe_flags_;
    int32_t buf_gid_ = IOURING_INVALID_BUFF_GID;
    uint16_t buf_bid_ = IOURING_INVALID_BUFF_GID;

}; // class IoCompletion

class DefaultCompletion : public IoCompletion
{
public:
    using promise_type = ssize_t;

public:
    DefaultCompletion()
        : fd_(-1)
    {}

    DefaultCompletion(fd_t &fd)
        : fd_(fd)
    {}

    void Complete(const cqe_t *cqe, char_t *) noexcept final override
    {
        InvokeCallback(cqe->res);
        result_.set_value(cqe->res);
        if (!More())
            delete this;
    }

    void SetException(std::exception_ptr eptr) noexcept override
    {
        result_.set_exception(eptr);
        if (!More())
            delete this;
    }

    std::future<promise_type> GetFuture()
    {
        return result_.get_future();
    }

    fd_t Fd()
    {
        return fd_;
    }

private:
    fd_t fd_ = -1;
    std::promise<promise_type> result_;
}; // class DefaultCompletion

class RecvSendMsgCompletion : public IoCompletion
{
public:
    using promise_type = ssize_t;

public:
    RecvSendMsgCompletion(fd_t &fd, const std::vector<iovec> &iov)
        : fd_(fd)
    {
        mh_.msg_iov = const_cast<iovec *>(iov.data());
        mh_.msg_iovlen = iov.size();
    }

    RecvSendMsgCompletion(fd_t &fd, const ::msghdr &msghdr)
        : fd_(fd)
        , mh_(msghdr)
    {}

    void Complete(const cqe_t *cqe, char_t *) noexcept final override
    {
        InvokeCallback(cqe->res);
        result_.set_value(cqe->res);
        if (!More())
            delete this;
    }

    void SetException(std::exception_ptr eptr) noexcept override
    {
        result_.set_exception(eptr);
        if (!More())
            delete this;
    }

    std::future<promise_type> GetFuture()
    {
        return result_.get_future();
    }

    const fd_t &GetFd() noexcept
    {
        return fd_;
    }

    ::msghdr *MsgHdr()
    {
        return &mh_;
    }

private:
    const fd_t fd_;
    ::msghdr mh_ = {};
    std::promise<promise_type> result_;
}; // class RecvSendMsgCompletion

class StatxCompletion : public IoCompletion
{
public:
    using promise_type = std::tuple<ssize_t, const char *, struct statx *>;

public:
    StatxCompletion(const char *pathname, struct statx *statxbuf)
        : pathname_(pathname)
        , statxbuf_(statxbuf)
    {}

    void Complete(const cqe_t *cqe, [[maybe_unused]] char_t *provided_buffer = nullptr) noexcept final override
    {
        InvokeCallback(cqe->res);
        result_.set_value(std::make_tuple(cqe->res, pathname_, statxbuf_));
        if (!More())
            delete this;
    }

    void SetException(std::exception_ptr eptr) noexcept override
    {
        result_.set_exception(eptr);
        if (!More())
            delete this;
    }

    std::future<promise_type> GetFuture()
    {
        return result_.get_future();
    }

private:
    const char *pathname_;
    struct statx *statxbuf_;
    std::promise<promise_type> result_;
}; // class StatxCompletion

class RwProvidedBufferCompletion : public IoCompletion
{
public:
    using promise_type = BufferInfo;

public:
    RwProvidedBufferCompletion(fd_t &fd, io_buf_gid_t gid)
        : fd_(fd)
    {
        buf_gid_ = gid;
        buffer_info_.buf_gid_ = buf_gid_;
    }

    void Complete(const cqe_t *cqe, char_t *provided_buffer = nullptr) noexcept final override
    {
        if (cqe->flags & IORING_CQE_F_BUFFER)
        {
            // IORING_CQE_F_BUFFER	If set, the upper 16 bits are the buffer ID
            buffer_info_.buf_bid_ = cqe->flags >> 16;

            if (provided_buffer == nullptr && cqe->res > 0)
            {
                IOURING_ASSERT(false,
                    fmt::format("cqe flag {:#x} indicates provided buffer and buffer size {} is greater than 0, but buffer is nullptr",
                        cqe->flags, cqe->res));
            }
        }
        if (IOURING_UNLIKELY(cqe->res == -ENOBUFS))
        {
            IOURING_ASSERT(false, fmt::format("cqe res {} indicates no free provided buffers", cqe->res));
        }
        buffer_info_.buffer_ = provided_buffer;
        buffer_info_.size_ = cqe->res;
        InvokeCallback(cqe->res);
        result_.set_value(buffer_info_);
        if (!More())
            delete this;
    }

    void SetException(std::exception_ptr eptr) noexcept override
    {
        result_.set_exception(eptr);
        if (!More())
            delete this;
    }

    std::future<promise_type> GetFuture()
    {
        return result_.get_future();
    }

    const fd_t &Fd()
    {
        return fd_;
    }

    const uint16_t &Bid() override
    {
        return buffer_info_.buf_bid_;
    }

    char_t *Buffer()
    {
        return buffer_info_.buffer_;
    }

    const ssize_t &Size()
    {
        return buffer_info_.size_;
    }

private:
    fd_t fd_;
    BufferInfo buffer_info_;
    std::promise<promise_type> result_;
}; // class ReadProvidedBufferCompletion

class SocketCompletion : public IoCompletion
{
public:
    using promise_type = fd_t;

public:
    SocketCompletion() = default;

    void Complete(const cqe_t *cqe, char_t *) noexcept final override
    {
        InvokeCallback(cqe->res);
        result_.set_value(cqe->res);
        if (!More())
            delete this;
    }

    void SetException(std::exception_ptr eptr) noexcept override
    {
        result_.set_exception(eptr);
        if (!More())
            delete this;
    }

    std::future<promise_type> GetFuture()
    {
        return result_.get_future();
    }

private:
    std::promise<promise_type> result_;
}; // class SocketCompletion

} // namespace iouring
