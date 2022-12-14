#pragma once

#include "iouring/io_uring.h"
#include "iouring/io_uring_utils.h"

namespace iouring {
class FileIo : private NonCopyable
{
public:
    explicit FileIo(IoUring &ring, os_fd_t) noexcept;
    FileIo(FileIo &&other) noexcept;
    FileIo &operator=(FileIo &&other) = delete;
    ~FileIo() noexcept;
    IoUring &GetRing() const;

    /**
     * Read file content to given buffer
     *
     */
    template<typename Func = IoCompletionCb>
    auto Read(void *buf, size_t count, off_t offset, Func &&handler_fn = NoOpIoCompletionCb);
    template<typename Func = IoCompletionCb>
    auto ReadVector(std::vector<iovec> &iovecs, off_t offset, Func &&handler_fn = NoOpIoCompletionCb);

    /**
     * Write buffer content to given file
     *
     */
    template<typename Func = IoCompletionCb>
    auto Write(void *buf, size_t count, off_t offset, Func &&handler_fn = NoOpIoCompletionCb);
    template<typename Func = IoCompletionCb>
    auto WriteVector(std::vector<iovec> &iovecs, off_t offset, Func &&handler_fn = NoOpIoCompletionCb);

    /**
     * Close the file
     *
     */
    Status Close();
    /**
     * Return the file descriptor currently associated with this object
     *
     * @return file descriptor associated, -1 otherwise
     */
    int GetFd() const;

    /**
     * Register associated fd with  with iouring
     *
     */
    Status RegisterFd();

    /**
     * Unregister associated fd
     *
     */
    Status UnregisterFd();

private:
    uint8_t SqeFlag();

private:
    IoUring &ring_;
    os_fd_t fd_;
    os_fd_t registered_fd_;
}; // class FileIo

//// Implementation

template<typename Func>
auto FileIo::Read(void *buf, size_t count, off_t offset, Func &&handler_fn)
{
    auto comp = std::make_unique<DefaultCompletion>(fd_);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_read(fd_, offset, buf, count, IoApiCommonVars());
    return ring_.SubmitIoRequest(std::move(req), comp.release());
}

template<typename Func>
auto FileIo::ReadVector(std::vector<iovec> &iovecs, off_t offset, Func &&handler_fn)
{
    auto comp = std::make_unique<DefaultCompletion>(fd_);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_readv(fd_, offset, iovecs, 0, IoApiCommonVars());
    return ring_.SubmitIoRequest(std::move(req), comp.release());
}


template<typename Func>
auto FileIo::Write(void *buf, size_t count, off_t offset, Func &&handler_fn)
{
    auto comp = std::make_unique<DefaultCompletion>(fd_);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_write(fd_, offset, buf, count, IoApiCommonVars());
    return ring_.SubmitIoRequest(std::move(req), comp.release());
}

template<typename Func>
auto FileIo::WriteVector(std::vector<iovec> &iovecs, off_t offset, Func &&handler_fn)
{
    auto comp = std::make_unique<DefaultCompletion>(fd_);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_writev(fd_, offset, iovecs, 0, IoApiCommonVars());
    return ring_.SubmitIoRequest(std::move(req), comp.release());
}

} // namespace iouring
