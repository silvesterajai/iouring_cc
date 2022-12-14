#pragma once
#include "iouring/buffer.h"
#include "iouring/io_uring_utils.h"
#include <cstdint>
#include <memory>

namespace iouring {
/// @class BufferPool
/// @brief Object containing a pointer to a piece of contiguous memory with a particular size.
/// The memory is divided into a set of contigous face of equal size, accessed by index or id.
/// clang-format off
///  	   │<-------------size------------------>│
/// 	   ├─────────────────────────────────────┤
///    id=0│                                     │
///        ├─────────────────────────────────────┤
///    id=1│                                     │
/// 	   ├─────────────────────────────────────┤
///    id=2│                                     │
/// 	   ├─────────────────────────────────────┤
///    id=n│                                     │
/// 	   └─────────────────────────────────────┘
/// clang-format on
/// Parameters associated with bufferpool class are
///     buffer_size - number of bytes allocated/associated with per index
///     buffer_count - number of contigious such memory
/// Total size of bytes associated with the bufferpool is (buffer_size * buffer_count)

class BufferPool
{
public:
    /// @brief Constructor for BufferPool
    /// @param buffer_size number of bytes per entry
    /// @param buffer_count number of memory buffers of size `buffer_size`
    ///
    /// @note Memory of size (`buffer_size` * `buffer_count`) is allocated and released on
    /// destruction of object.
    explicit BufferPool(uint32_t buffer_size, uint32_t buffer_count)
        : buffer_count_(buffer_count)
        , buffer_size_(buffer_size)
        , data_(buffer_count_ * buffer_size_)
    {}

    /// @brief Constructor for BufferPool
    /// @param buffer_size number of bytes per entry
    /// @param buffer_count number of memory buffers of size `buffer_size`
    /// @param bufs memory pointer
    ///
    /// \note The passed memory should be kept alive until the lifetime of this object.
    explicit BufferPool(uint32_t buffer_size, uint32_t buffer_count, char_t *bufs)
        : buffer_count_(buffer_count)
        , buffer_size_(buffer_size)
        , data_(bufs, buffer_count_ * buffer_size_)
    {}

    /// @brief Returns the pointer to buffer indexed by given `buffer_id`.
    /// `buffer_id` should be less that `buffer_count`.
    /// @param buffer_id buffer index
    /// @return memory pointer
    char_t *Get(uint32_t buffer_id)
    {
        if (IOURING_UNLIKELY(buffer_id >= buffer_count_))
        {
            IOURING_ASSERT(false, fmt::format("Buffer id {} should be less than buffer count {}", buffer_id, buffer_count_));
        }
        auto offset = buffer_id * buffer_size_;
        return std::addressof(data_.data()[offset]);
    }

    /// @brief Returns the starting memory pointer associated with bufferpool.
    /// @return memory pointer
    char_t *Data()
    {
        return data_.data();
    }

    /// @brief Returns number of buffers in bufferpool.
    /// @return number of buffers
    const uint32_t &Count()
    {
        return buffer_count_;
    }

    /// @brief Returns the size of each memory buffer in bufferpool.
    /// @return size of each buffer in bufferpool
    const uint32_t &Size()
    {
        return buffer_size_;
    }

private:
    uint32_t buffer_count_;
    uint32_t buffer_size_;
    Buffer<char_t> data_;

}; // class BufferPool
} // namespace iouring