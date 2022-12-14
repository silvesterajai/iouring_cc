/*
 * Copyright 2021 ScyllaDB
 */

#pragma once
#include "iouring/io_queue.h"
#include "iouring/io_request.h"
#include "iouring/io_uring_utils.h"
#include "iouring/logger.h"
#include "iouring/status.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <queue>

using namespace std::chrono_literals;

namespace iouring {

class IoCompletion;
class IoSink;

/// Holds the io request and io completion which will be stored in `IoSink`
class PendingIoRequest
{
    friend class IoSink;

public:
    PendingIoRequest(std::unique_ptr<IoRequest> req, IoCompletion *desc) noexcept
        : io_request_(std::move(req))
        , completion_(desc)
    {}

private:
    std::unique_ptr<IoRequest> io_request_;
    IoCompletion *completion_;
};

/// Holds `IoPendingIoRequest` which will be submitted/drained to kernel io_uring submission queue
class IoSink
{
public:
    IoSink([[maybe_unused]] size_t initial_size_estimate = (64 * 4))
        : stop_(false)
    {}

    ~IoSink()
    {
        stop_ = true;
        condition_.notify_all();
    }

    /// Drain `PendingIoRequest` to kernel io_uring.
    /// Fn should return whether the request was consumed and draining should try to drain more.
    /// Number of requestes drained is limited to `max_drain` parameter. Additional requests will be drained in next attempt.
    /// If there is any request which is submitted to kernel but is not completed or processed, then `pending_sqe` will be greater than 0.
    /// If this value is greater than 0 this function will not wait for new io request to be available before draining them. Instead
    /// priority will be given to process the completion queue. But if there is any pending io in `pending_io_`, those requestes will be
    /// submitted to kernel (again limited to `max_drain`).
    template<typename Fn>
    ssize_t Drain(Fn &&consume, const size_t &max_drain, const std::atomic<ssize_t> &pending_sqe)
    {
        size_t current_drained = 0;
        {
            std::unique_lock<std::mutex> lock(pending_io_mutex_);
            LOG_TRACE("Pending io request size: {}, stop: {}, pending_sqe: {}", pending_io_.size(), stop_, pending_sqe);
            condition_.wait(lock, [this, &pending_sqe] { return this->stop_ || !this->pending_io_.empty() || pending_sqe > 0; });

            while (!stop_)
            {
                if (pending_io_.empty())
                    break;
                // Peek front/first element
                auto req = std::move(pending_io_.front());
                pending_io_.pop();

                // Invoke consume. If returned false, then do not drain any more
                if (!consume(std::move(req.io_request_), req.completion_))
                    break;

                // Increment the drained count
                ++current_drained;

                // Number of request drained is limited by `max_drain`
                if (current_drained >= max_drain)
                    break;
            }
        }
        // If stopped then return with -kClosed value
        if (stop_)
            return -(long)StatusCode::kClosed;

        // Return the drained count
        return current_drained;
    }

    /// Enqueue io request to `pending_io_`. The request is not submitted to kernel submission queue. This request will be drained and
    /// submitted to kernel submission queue in later stage.
    void EnqueueIoRequest(std::unique_ptr<IoRequest> req, IoCompletion *desc) noexcept
    {
        IOURING_ASSERT(!stop_, "IoSink is stopped. Cannot accept request after stopping io_sink.");
        {
            std::unique_lock<std::mutex> lock(pending_io_mutex_);
            pending_io_.emplace(std::move(req), desc);
        }
        LOG_TRACE("IoSink size after insert{}", pending_io_.size());
        condition_.notify_one();
    }

    void Stop() noexcept
    {
        stop_ = true;
        condition_.notify_all();
    }

private:
    // pending request queue
    IoQueue<PendingIoRequest> pending_io_;

    // synchronization
    std::mutex pending_io_mutex_;
    std::condition_variable condition_;

    // state
    bool stop_;
};
} // namespace iouring
