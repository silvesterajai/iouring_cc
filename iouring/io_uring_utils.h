#pragma once
#include "iouring/assert.h"
#include "iouring/logger.h"
#include <array>
#include <execinfo.h>
#include <fcntl.h>
#include <liburing/io_uring.h>
#include <linux/time_types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <time.h>
#include <type_traits>
#include <unistd.h>

namespace iouring {

using offset_t = uint64_t;
using fd_t = int;
using char_t = char;
using io_size_t = std::size_t;
using flag_t = int;
using msghdr_t = ::msghdr;
using socket_addr_t = ::sockaddr;
using socket_len_t = socklen_t;
using index_t = int;
using IoUringFlags = uint32_t;
using os_fd_t = int;
using Operation = io_uring_op;
using pointer_t = uint64_t;
using io_buf_gid_t = int32_t;
using io_buf_bid_t = uint16_t;
using cqe_t = struct io_uring_cqe;
using sqe_t = struct io_uring_sqe;

#define IOURING_LIKELY(expr) __builtin_expect(!!(expr), 1)
#define IOURING_UNLIKELY(expr) __builtin_expect(!!(expr), 0)

static constexpr int IOURING_INVALID_BUFF_GID = 0;
static constexpr int IOURING_INVALID_SOCKET = -1;
static constexpr int IOURING_INVALID_FILEINDEX = -1;
static constexpr int IOURING_DEFAULT_MAX_FD_SLOTS = 0;
static constexpr int IOURING_DEFAULT_CQE_ENTRIES = 64;
static constexpr int IOURING_DEFAULT_MAX_DRAINED = 10;

// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
inline uint32_t round_up_power_of_two(uint32_t num)
{
    if (num == 0)
    {
        return 0;
    }
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    return num + 1;
}

/**
 * Fill an iovec struct using buf & size
 */
constexpr inline iovec to_iov(void *buf, size_t size) noexcept
{
    return {buf, size};
}

/**
 * Fill an iovec struct using std::array
 */
template<size_t N>
constexpr inline iovec to_iov(std::array<char, N> &array) noexcept
{
    return to_iov(array.data(), array.size());
}

/** Convert errno to exception
 * @throw std::runtime_error / std::system_error
 * @return never
 */
[[noreturn]] inline void panic(std::string str, int err)
{
#ifndef NDEBUG
    // https://stackoverflow.com/questions/77005/how-to-automatically-generate-a-stacktrace-when-my-program-crashes
    void *array[32];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 32);

    // print out all the frames to stderr
    fprintf(stderr, "Error: errno %d:\n", err);
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    // __asm__("int $3");
#endif

    throw std::system_error(err, std::generic_category(), str);
}

struct panic_on_err
{
    panic_on_err(std::string command, bool use_errno)
        : command_(command)
        , use_errno_(use_errno)
    {}

    std::string command_;
    bool use_errno_;
};

inline int operator|(int ret, panic_on_err &&poe)
{
    if (ret < 0)
    {
        if (poe.use_errno_)
        {
            panic(poe.command_, errno);
        }
        else
        {
            if (ret != -ETIME)
                panic(poe.command_, -ret);
        }
    }
    return ret;
}

/**
 * Run the command passed until it dose not fail with EINTR
 *
 * @param op command to be executed
 */

template<typename Op>
auto syscall_retry(Op &&op)
{
    for (;;)
    {
        const auto result = op();
        if (result != -1 || errno != EINTR)
        {
            return result;
        }
    }
}

template<typename DurationT>
inline void chrono_to_kts(DurationT t, __kernel_timespec &ts) noexcept
{
    ts.tv_sec = std::chrono::floor<std::chrono::seconds>(t).count();
    ts.tv_nsec = (t % std::chrono::seconds(1)).count();
}

inline void usec_to_timespec(uint64_t usec, __kernel_timespec &ts) noexcept
{
    int64_t usec_rounded_to_sec = usec / 1000000L * 1000000L;
    long long nsec = (usec - usec_rounded_to_sec) * 1000L;
    ts.tv_sec = usec_rounded_to_sec / 1000000L;
    ts.tv_nsec = nsec;
}

inline void msec_to_timespec(uint64_t msec, __kernel_timespec &ts) noexcept
{
    int64_t msec_rounded_to_sec = msec / 1000L * 1000L;
    long long nsec = (msec - msec_rounded_to_sec) * 1000000L;
    ts.tv_sec = msec_rounded_to_sec / 1000L;
    ts.tv_nsec = nsec;
}

typedef struct BufferInfo
{
    char_t *buffer_;
    ssize_t size_;
    uint32_t buf_gid_;
    uint16_t buf_bid_;
} BufferInfo;

template<typename Type, std::size_t ExpectedSize, std::size_t ActualSize = 0>
struct validate_size : std::true_type
{
    static_assert(ActualSize == ExpectedSize, "actual size does not match expected size");
};

template<typename Type, std::size_t ExpectedSize>
struct validate_size<Type, ExpectedSize, 0> : validate_size<Type, ExpectedSize, sizeof(Type)>
{};

#define STATIC_ASSERT_SIZE(T, sz) static_assert(validate_size<T, sz>::value, "")

} // namespace iouring
