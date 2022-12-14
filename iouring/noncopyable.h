#pragma once

namespace iouring {
/// @brief Class where copy constructor and copy assignment operator are deleted.
/// To make any class non-copyable derive from this class.
class NonCopyable
{
public:
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable &operator=(const NonCopyable &) = delete;

protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
}; // class NonCopyable

/// @brief Class where move constructor and move assignment operator are deleted.
/// To make any class non-copyable and non-movable derive from this class.
class NonCopyableMovable : public NonCopyable
{
public:
    NonCopyableMovable(NonCopyableMovable &&) = delete;
    NonCopyableMovable &operator=(NonCopyableMovable &&) = delete;

protected:
    NonCopyableMovable() = default;
    ~NonCopyableMovable() = default;
}; // class NonCopyableMovable

} // namespace iouring
