#pragma once

#include <type_traits>
#include <utility>

namespace iouring {
template<typename Func>
class [[nodiscard("Deffered action is not assigned")]] DeferredAction
{
public:
    static_assert(std::is_nothrow_move_constructible<Func>::value, "Func(Func&&) must be noexcept");

    DeferredAction(Func && func) noexcept
        : func_(std::move(func))
    {}

    DeferredAction(DeferredAction && other) noexcept
        : func_(std::move(other.func_))
        , cancelled_(other.cancelled_)
    {
        other.cancelled_ = true;
    }

    DeferredAction &operator=(DeferredAction &&other) noexcept
    {
        if (this != &other)
        {
            this->~DeferredAction();
            new (this) DeferredAction(std::move(other));
        }
        return *this;
    }

    ~DeferredAction()
    {
        if (!cancelled_)
        {
            func_();
        }
    }

    void Cancel()
    {
        cancelled_ = true;
    }

private:
    Func func_;
    bool cancelled_ = false;
}; // DeferredAction
} // namespace iouring
