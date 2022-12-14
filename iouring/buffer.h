#pragma once
#include "iouring/io_uring_utils.h"
#include <cstddef>   // std::size_t
#include <cstring>   // std::memcpy, std::memcmp, std::memset, std::memchr
#include <stdexcept> // std::runtime_error

namespace iouring {

/// @class Buffer
/// @brief Object containing a pointer to a piece of contiguous memory with a particular size
/// @tparam T Object type representing the buffer. It should be of size 1 (e.g. char, uint8_t)
///
/// Parameters associated with buffer class are
///     capacity - number of bytes allocated/associated with this object
///     size - number of bytes that is currently valid. Size if always <= capacity
///     own_mem - indicates if the associated contiguous memory is owned by this object.
///               If owned by this object, then it will be freed when the object is destructed.
template<typename T>
class Buffer
{
    static_assert(sizeof(T) == 1, "must buffer stream of bytes");

public:
    using size_type = std::size_t;
    using pointer = T *;

public:
    /// @brief Construct buffer with own memory. new T(capacity) will be invoked.
    ///
    /// @param capacity buffer size
    Buffer(std::size_t capacity)
        : capacity_(capacity)
        , index_(0)
        , ptr_(new T[capacity])
        , own_mem_(true)
    {}

    /// @brief Construct buffer from memory without constructing/allocating new memory
    ///
    /// @param data a memory pointer
    /// @param length buffer size
    ///
    /// \note The passed memory should be kept alive until the lifetime of this object
    explicit Buffer(T *data, std::size_t length)
        : capacity_(length)
        , index_(length)
        , ptr_(data)
        , own_mem_(false)
    {}

    /// @brief Construct buffer from memory by constructing/allocating new memory.
    /// new T(capacity) will be invoked.
    ///
    /// @param data a memory pointer
    /// @param length buffer size
    ///
    /// @note Changing the content via this object does not affect/change the original memory
    /// passed
    explicit Buffer(const T *ptr_mem, std::size_t length)
        : capacity_(length)
        , index_(length)
        , ptr_(new T[length])
        , own_mem_(true)
    {
        if (capacity_)
        {
            std::memcpy(ptr_, ptr_mem, length);
        }
    }

    /// @brief Copy constructor
    /// @param other buffer object
    Buffer(const Buffer &other)
        : capacity_(other.capacity_)
        , index_(other.index_)
        , ptr_(new T[other.index_])
        , own_mem_(false)
    {
        if (index_)
        {
            std::memcpy(ptr_, other.ptr_, index_);
        }
    }

    /// @brief Assignment operator
    /// @param other buffer object
    /// @return copy of `other` buffer object
    Buffer &operator=(const Buffer &other)
    {
        if (this != &other)
        {
            Buffer tmp(other);
            swap(tmp);
        }
        return *this;
    }

    /// Desctructor
    ~Buffer()
    {
        if (own_mem_)
            delete[] ptr_;
    }

    /// @brief Resize the buffer to new capacity
    /// Resize the buffer to new capacity if following conditions are satisfied
    ///     - new_capacity is greater than current capacity
    ///     - this object owns the associated memory
    /// If memory is not owned by the object then std::runtime_error is thrown
    /// By default the content is preserved. If its not needed to preserve the content,
    /// then pass `preserve_content` as false. In this condition its same behavior as
    /// creating new buffer class with capacity equal to `new_capacity` and size will be set to 0.
    ///
    /// @param new_capacity new capacity of buffer
    /// @param preserve_content preserve the original content if set to true
    void resize(size_type new_capacity, bool preserve_content = true)
    {
        if (!own_mem_)
            throw std::runtime_error("Cannot resize buffer not owned");
        if (new_capacity > capacity_)
        {
            T *ptr = new T[new_capacity];
            if (preserve_content)
            {
                // since content is preserved, index_ value is not changed
                std::memcpy(ptr, ptr_, index_ * sizeof(T));
            }
            else
            {
                // content is not preserved, index_ is reset to 0
                index_ = 0;
            }
            delete[] ptr_;
            ptr_ = ptr;
            capacity_ = new_capacity;
        }
    }

    /// @brief Set the buffer capacity to new value
    /// Resize the buffer to new capacity if following conditions are satisfied
    ///     - this object owns the associated memory
    /// If memory is not owned by the object then std::runtime_error is thrown
    /// By default the content is preserved. If its not needed to preserve the content,
    /// then pass `preserve_content` as false. In this condition its same behavior as
    /// creating new buffer class with capacity equal to `new_capacity` and size will be set to 0.
    ///
    /// @param new_capacity new capacity of buffer
    /// @param preserve_content preserve the original content if set to true
    ///
    /// @note If the new_capacity is less than original capacity, then there will be data loss.
    /// `resize` API will ensure that capacity is changed only if `new_capacity` is greater than current.
    void set_capacity(size_type new_capacity, bool preserve_content = true)
    {
        if (!own_mem_)
            throw std::runtime_error("Cannot resize buffer not owned");
        if (new_capacity != capacity_)
        {
            T *ptr = new T[new_capacity];
            if (preserve_content)
            {
                size_type new_sz = index_ < new_capacity ? index_ : new_capacity;
                std::memcpy(ptr, ptr_, new_sz * sizeof(T));
                index_ = new_sz;
            }
            else
            {
                index_ = 0;
            }
            delete[] ptr_;
            ptr_ = ptr;
            capacity_ = new_capacity;
        }
    }

    /// @brief Copy input memory to memory associated with this object
    /// Buffer will be resized (if needed) to accomodate new data
    /// @param data input memory
    /// @param sz size of input memory
    void assign(const T *data, size_type sz)
    {
        if (sz == 0)
            return;
        if (sz > capacity_)
            resize(sz, false);
        std::memcpy(ptr_, data, sz * sizeof(T));
        index_ = sz;
    }

    /// @brief Append data to buffer
    /// Buffer will be resized (if needed) to accomodate new data
    /// @param data input data
    /// @param sz size of input data
    void append(const T *data, size_type sz)
    {
        if (sz == 0)
            return;
        resize(index_ + sz, true);
        std::memcpy(ptr_ + index_, data, sz * sizeof(T));
        index_ += sz;
    }

    /// @brief Append input buffer data to this object
    /// @param buf input buffer
    void append(const Buffer &buf)
    {
        append(buf.begin(), buf.size());
    }

    /// @brief Consume `sz` data from the front of buffer.
    /// If `sz` is less than `size`, then (size - sz) data from end will be shifted to front
    /// @param sz size of data tobe consumed
    ///
    /// @note Ensure `sz` should be less than buffers current size
    void consumefront(const size_type sz)
    {
        IOURING_ASSERT(sz <= index_, "consume size should be less than available size");
        if (sz < index_)
            memmove(ptr_, ptr_ + sz, index_ - sz);
        index_ -= sz;
    }

    /// @brief Returns capacity of buffer
    /// @return capacity of buffer
    size_type capacity() const
    {
        return capacity_;
    }

    /// @brief Swap the buffer content between the buffer objects
    /// @param other other buffer object
    void swap(Buffer &other)
    {
        using std::swap;

        swap(ptr_, other.ptr_);
        swap(capacity_, other.capacity_);
        swap(index_, other.index_);
    }

    bool operator==(const Buffer &other) const
    {
        if (this != &other)
        {
            if (index_ == other.index_)
            {
                if (std::memcmp(ptr_, other.ptr_, index_ * sizeof(T)) == 0)
                {
                    return true;
                }
            }
            return false;
        }

        return true;
    }

    bool operator!=(const Buffer &other) const
    {
        return !(*this == other);
    }

    /// @brief Reset the size to 0.
    void clear()
    {
        // if (!own_mem_)
        //     throw std::runtime_error("Cannot clear buffer not owned");
        // std::memset(ptr_, 0, index_ * sizeof(T));
        index_ = 0;
    }

    /// @brief Returns current size of the buffer
    /// @return Current size of the buffer
    std::size_t size() const
    {
        return index_;
    }

    T *begin()
    {
        ptr_;
    }

    T *begin() const
    {
        return ptr_;
    }

    T *end()
    {
        return (ptr_ + index_);
    }

    T *end() const
    {
        return (ptr_ + index_);
    }

    /// @brief Check the the buffer is empty
    /// @return true if size of buffer is 0, false otherwise
    bool empty()
    {
        return index_ == 0;
    }

    T *data()
    {
        return ptr_;
    }

    T *data() const
    {
        return ptr_;
    }

    T &operator[](size_type index)
    {
        IOURING_ASSERT(index < index_, "index check failed");
        return ptr_[index];
    }

    T &operator[](size_type index) const
    {
        IOURING_ASSERT(index < index_, "index check failed");
        return ptr_[index];
    }

private:
    size_type capacity_;
    size_type index_;
    pointer ptr_;
    bool own_mem_ = false;
}; // class Buffer
} // namespace iouring
