#include "examples/fileio/fileio.h"

using namespace iouring;

FileIo::FileIo(IoUring &ring, os_fd_t fd) noexcept
    : ring_(ring)
    , fd_(fd)
    , registered_fd_(-1)
{}

FileIo::FileIo(FileIo &&other) noexcept
    : ring_(other.ring_)
    , fd_(other.fd_)
    , registered_fd_(other.registered_fd_)
{}

FileIo::~FileIo() noexcept
{
    [[maybe_unused]] auto result = Close();
}

IoUring &FileIo::GetRing() const
{
    return ring_;
}

Status FileIo::Close()
{
    if (fd_ != -1)
    {
        [[maybe_unused]] auto result = UnregisterFd();
        syscall_retry([&] { return ::close(fd_); });
    }
    return Status::OkStatus();
}

Status FileIo::RegisterFd()
{
    if (registered_fd_ != -1)
    {
        // Already registed fd
        return Status::OkStatus();
    }
    auto ret = ring_.RegisterFile(fd_);
    if (!ret.Ok())
    {
        return ret;
    }
    registered_fd_ = fd_;
    return Status::OkStatus();
}

Status FileIo::UnregisterFd()
{
    if (registered_fd_ == -1)
    {
        // Not registered
        return Status::OkStatus();
    }
    auto ret = ring_.UnregisterFile(registered_fd_);
    if (!ret.Ok())
    {
        return ret;
    }
    registered_fd_ = -1;
    return Status::OkStatus();
}

int FileIo::GetFd() const
{
    return fd_;
}

uint8_t FileIo::SqeFlag()
{
    uint8_t iflags = 0;
    if (registered_fd_ != -1)
        iflags |= IOSQE_FIXED_FILE;

    return iflags;
}
