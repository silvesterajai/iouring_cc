#pragma once
#include "assert.h"                 // for IOURING_ASSERT
#include "iouring/buffer_pool.h"    // for BufferPool
#include "iouring/io_completion.h"  // for IoFuture
#include "iouring/io_sink.h"        // for IoSink
#include "iouring/io_uring_utils.h" // for IOURING_INVALID_BUFF_GID, round...
#include "iouring/logger.h"         // for LOG_DEBUG, LOG_ERROR, LOG_TRACE, LOG_WARN
#include "iouring/noncopyable.h"    // for NonCopyable
#include "iouring/status.h"         // for Status
#include "iouring/threadpool.h"     // for ThreadPool
#include "liburing.h"               // for io_uring_cq_ready, io_uring_sq_...
#include <atomic>                   // for atomic
#include <cstdint>                  // for uint32_t, int32_t, uint64_t
#include <exception>                // for current_exception
#include <liburing/io_uring.h>      // for io_uring_op, IORING_OP_ACCEPT
#include <linux/time_types.h>       // for __kernel_timespec
#include <map>                      // for map, operator!=, _Rb_tree_itera...
#include <memory>                   // for unique_ptr, make_unique
#include <mutex>                    // for mutex, lock_guard
#include <stdexcept>                // for runtime_error
#include <sys/types.h>              // for ssize_t, size_t
#include <utility>                  // for pair, move, addressof, make_pair
#include <vector>                   // for vector

namespace iouring {

/**
 * io_uring doesn’t expose any user-visible features.
 * One option is to invoke io_uring_queue_init API and check the return value.
 *
 * return true is io_uring is supported, false otherwise.
 */
bool IsIoUringSupported();

// Forward declaration
class IoUring;

/// @brief IoUring build params.
/// This class holds the parameters that will be used to construct IoUring instance.
class IoUringParams
{
    friend class IoUring;

public:
    IoUringParams()
        : registered_buffers_(nullptr, 0)
    {
        // Set default cqe wait time to 0.
        SetCqeWaitTimeout(0);
        // Minimum required operations that should be supported by kernel.
        required_ops_ = {
            IORING_OP_POLL_ADD,
            IORING_OP_POLL_REMOVE,
            IORING_OP_READV,
            IORING_OP_WRITEV,
            IORING_OP_FSYNC,
            IORING_OP_SENDMSG,
            IORING_OP_RECVMSG,
            IORING_OP_ACCEPT,
            IORING_OP_CONNECT,
            IORING_OP_READ,
            IORING_OP_WRITE,
            IORING_OP_SEND,
            IORING_OP_RECV,
            IORING_OP_EPOLL_CTL,
        };
    }

    IoUringParams &SetIoPoll()
    {
        // Perform busy-waiting for an I/O completion, as opposed to getting notifications
        // via an asynchronous IRQ (Interrupt Request).
        flag_ |= IORING_SETUP_IOPOLL;
        return *this;
    }

    IoUringParams &SetSqPoll()
    {
        // When this flag is specified, a kernel thread is created to perform
        // submission queue polling. An io_uring instance configured in this
        // way enables an application to issue I/O without ever context switching
        // into the kernel
        flag_ |= IORING_SETUP_SQPOLL;
        return *this;
    }

    IoUringParams &SetFlag(uint32_t flag)
    {
        flag_ = flag;
        return *this;
    }

    IoUringParams &SetCqeWaitTimeout(uint32_t cq_wait_timeout_us)
    {
        usec_to_timespec(cq_wait_timeout_us, cqe_wait_timeout_);
        return *this;
    }

    IoUringParams &SetMaxFdSlots(uint32_t max_fd_slots)
    {
        max_fd_slots_ = max_fd_slots;
        return *this;
    }

    IoUringParams &SetCqeEntries(uint32_t entries)
    {
        cq_entries_ = entries;
        cq_entries_ = round_up_power_of_two(cq_entries_);
        // IORING_SETUP_CQSIZE:
        //    Create the completion queue with struct io_uring_params.cq_entries entries.
        if (cq_entries_ > 0)
        {
            flag_ |= IORING_SETUP_CQSIZE;
        }
        return *this;
    }

    IoUringParams &SetAttachWqFd(int fd)
    {
        if (fd >= 0)
        {
            wq_attach_fd_ = fd;
            // IORING_SETUP_ATTACH_WQ
            //    This flag should be set in conjunction with struct io_uring_params.wq_fd being
            //    set to an existing io_uring ring file descriptor. When set, the io_uring
            //    instance being created will share the asynchronous worker thread backend of the
            //    specified io_uring ring, rather than create a new separate thread pool.
            flag_ |= IORING_SETUP_ATTACH_WQ;
        }
        return *this;
    }

    IoUringParams &EnableSubConsumeThread()
    {
        start_sub_consume_thread_ = true;
        return *this;
    }

    IoUringParams &DisableSubConsumeThread()
    {
        start_sub_consume_thread_ = false;
        return *this;
    }

    IoUringParams &RegisterEventFd(bool value)
    {
        register_event_fd_ = value;
        return *this;
    }

    IoUringParams &SetMaxCqeDrain(uint32_t max_cqe)
    {
        max_cqe_to_drain_ = max_cqe;
        return *this;
    }

    IoUringParams &ShouldSupportFeatFastPoll()
    {
        features_needed_ |= IORING_FEAT_FAST_POLL;
        return *this;
    }

    IoUringParams &ShouldSupportOpProvideBuffers()
    {
        required_ops_.push_back(IORING_OP_PROVIDE_BUFFERS);
        return *this;
    }

    IoUringParams &AddProvidedBuffer(io_buf_gid_t gid, uint32_t buffer_size, uint32_t buffer_count)
    {
        IOURING_ASSERT(gid != IOURING_INVALID_BUFF_GID, fmt::format("gid should not be equal to {}", IOURING_INVALID_BUFF_GID));
        auto it = provided_buffers_.find(gid);
        if (it != provided_buffers_.end())
            provided_buffers_.erase(it);
        provided_buffers_.emplace(gid, std::make_pair(buffer_size, buffer_count));
        return *this;
    }

    IoUringParams &AddProvidedBuffer(io_buf_gid_t gid, uint32_t buffer_size, uint32_t buffer_count, char_t *bufs)
    {
        IOURING_ASSERT(gid != IOURING_INVALID_BUFF_GID, fmt::format("gid should not be equal to {}", IOURING_INVALID_BUFF_GID));
        auto it = pre_allocate_provided_buffers_.find(gid);
        if (it != pre_allocate_provided_buffers_.end())
        {
            IOURING_ASSERT(false, fmt::format("AddProvidedBuffer, duplicate group id entry for {}", gid));
        }
        pre_allocate_provided_buffers_.emplace(gid, new BufferPool(buffer_size, buffer_count, bufs));
        return *this;
    }

    IoUringParams &RegisterBuffers(const struct iovec *iovecs, const uint32_t nr_iovecs)
    {
        // Not supporting io_uring_register_buffers_sparse.
        IOURING_ASSERT(iovecs, "iovecs cannot be nullptr");
        IOURING_ASSERT(nr_iovecs, "nr_iovecs cannot be 0");
        registered_buffers_.first = iovecs;
        registered_buffers_.second = nr_iovecs;
        return *this;
    }

    std::unique_ptr<IoUringParams> Build()
    {
        return std::make_unique<IoUringParams>(*this);
    };

private:
    uint32_t flag_ = 0;
    struct __kernel_timespec cqe_wait_timeout_;
    uint32_t max_fd_slots_ = IOURING_DEFAULT_MAX_FD_SLOTS;
    uint32_t cq_entries_ = IOURING_DEFAULT_CQE_ENTRIES;
    int wq_attach_fd_ = -1;
    bool start_sub_consume_thread_ = true;
    bool register_event_fd_ = true;
    uint32_t max_cqe_to_drain_ = IOURING_DEFAULT_MAX_DRAINED;
    uint32_t features_needed_ = 0;
    std::vector<io_uring_op> required_ops_;
    std::map<io_buf_gid_t /*gid*/, std::pair<uint32_t /*buffer_size*/, uint32_t /*buffer_count*/>> provided_buffers_;
    std::map<io_buf_gid_t /*gid*/, BufferPool *> pre_allocate_provided_buffers_;
    std::pair<const struct iovec *, uint32_t> registered_buffers_;
};

/**
 * Holds list of file fds register with io_uring
 *
 */
class IoUringFileFdContainer : private NonCopyable
{
    static constexpr int INVALID_ID = -1;
    friend class IoUring;

public:
    IoUringFileFdContainer(int max_fds);
    ~IoUringFileFdContainer() = default;

private:
    /**
     * Lookup fd is present in the list.
     *
     * @return return index if fd is present. If not present then return -1
     */
    int LookUp(int fd);

    /**
     * Add fd to list
     *
     * @param fd fd to add to list
     * @return return index associated with fd, -1 if limit exceeds
     *         return existing index if fd is already associated.
     */
    int Add(int fd);

    /**
     * Clear all fds from the list
     *
     * @return None
     */
    void CleanUp();

    /**
     * Remove fd
     *
     * @param fd fd to be removed from list
     */
    void Remove(int fd);

    /**
     * Get the fds,count pair to be used by io_uring_register_files
     *
     * @return std::pair<int*, int>
     */
    std::pair<int *, int> Fds();

    /**
     * Return size of fds
     *
     * @return size of fds
     */
    size_t Size()
    {
        return fd_to_index_.size();
    }

    /**
     * Max fds that can be registered
     *
     * @return int capacity
     */
    size_t Capacity()
    {
        return max_size_;
    }

private:
    int max_size_;
    std::map<int, int> fd_to_index_;
    std::vector<int> free_ids_;
};

class IouringStats final
{
public:
    IouringStats()
    : sqe_flags_(8, 0)
    , ops_(Operation::IORING_OP_LAST, 0)
    {}
    ~IouringStats() = default;
    void UpdateSqeStats(const sqe_t *sqe);
    void UpdateCqeStats(const cqe_t *cqe);

public:
//flags
    std::vector<uint32_t> sqe_flags_;
//ops
    std::vector<uint32_t> ops_;

}; // class IouringStats

/**
 * C++ wrapper class around liburing
 */
class IoUring : private NonCopyable
{

private:
    using sqe_t = struct io_uring_sqe;
    using cqe_t = struct io_uring_cqe;

public:
    /// ctors
    IoUring(const IoUringParams &params, uint32_t wq_fd = 0);
    ~IoUring();

    /**
     * Return the current configured flag associated with io_uring
     *
     * @return flags
     */
    const IoUringFlags &Flag()
    {
        return ring_.flags;
    }

    /**
     * Check if eventfd is registered
     *
     * @return true if eventfd is registered, false otherwise.
     */
    bool IsEventFdRegistered();

    /**
     * Returns if the event fd
     *
     * @return event fd if registered, -1 otherwise
     */
    int EventFd()
    {
        return event_fd_;
    }

    /**
     * CHeck if required features are supported
     *
     * @params required_ops list of required operations
     * @return true if all required features are supported, false otherwise.
     *
     */
    bool OpsSupported(std::vector<io_uring_op> required_ops);

    /**
     * Check if required features are supported.
     *
     * @param required_features bit mapping of required features.
     * @return true if the required features are supported, false otherwise.
     *
     */
    bool FeaturesSupported(int required_features);

    /**
     * Register file for i/o
     *
     * @param fd fd to register
     * @return 0 if success, non-zero othrewise
     */
    Status RegisterFile(int fd);

    /**
     * Register files for i/o
     *
     * @param std::vector<fd_t> fds to register
     * @return 0 if success, non-zero othrewise
     */
    Status RegisterFiles(std::vector<fd_t> &fds);

    /**
     * Register files for i/o. This APIs is different from `RegisterFiles` such that, this object does not stores mapping betweed `fd` and
     * `index`. If `is_update` is set to true then `io_uring_register_files_update` will be invoked.
     *
     * @param std::vector<fd_t> fds to register
     * @param index starting index to register the file descriptors
     * @param is_update true if updating registered file(s), false other wise.
     * @return rumber of fds registered if success, -ERROR othrewise
     */
    int RegisterFilesExternal(std::vector<fd_t> &fds, int index = 0, bool is_update = false);

    /**
     * Unregister file for i/o
     *
     * @param fd fd to unregister
     * @return 0 if success, non-zero othrewise
     */
    Status UnregisterFile(int fd);

    /**
     * Unregister files
     *
     * @return 0 if success, non-zero othrewise
     */
    Status UnregisterFiles();

    /**
     * Register buffers for i/o
     *
     * @param iovecs array of iovecs to register
     * @param nr_iovecs size of iovecs array
     * @return None
     */
    void RegisterBuffers(const struct iovec *iovecs, uint32_t nr_iovecs);

    /**
     * Unregister buffers
     *
     * @return None
     */
    void UnregisterBuffers();

    /**
     * Submit io request for kernel processing
     *
     */
    template<typename Completion>
    auto SubmitIoRequest(std::unique_ptr<IoRequest> req, Completion *completion);

    /**
     * Submit the prepared SQEs to kernel uring
     *
     */
    Status FlushSubmissionRing();

    /**
     * Process completion ring. This call will process all the completed cqe.
     * @note This should be only invoked if internal thread is not started.
     */
    Status ProcessComplitionsRing();

    /**
     * Removes the buffer identified by buf_gid from buffer pool.
     * This should be invoked after cqe handler in response to io_uring_prep_remove_buffers call.
     * Do not invoke this function directly without io_uring_prep_remove_buffers.
     *
     * @param int32_t buffer group id
     * @return Status sucess if buffer is removed, failure otherwise
     */
    Status EraseBufferFromPool(io_buf_gid_t gid);

    /**
     * Number of unconsumed entries are ready in the CQ ring
     *
     * @return uint32_t count
     */
    uint32_t CqeReady()
    {
        return ::io_uring_cq_ready(Ring());
    }

    /**
     * Returns number of unconsumed (if SQPOLL) or unsubmitted entries exist in the SQ ring
     *
     * @return uint32_t count
     */
    uint32_t SqeReady()
    {
        return ::io_uring_sq_ready(Ring());
    }

    /**
     * Returns how much space is left in the SQ ring.
     *
     * @return uint32_t count
     */
    uint32_t SqeSpaceLeft()
    {
        return ::io_uring_sq_space_left(Ring());
    }

#ifdef IOURING_STATS
    const IouringStats &Stats()
    {
        return stats_;
    }
#endif

private:
    /**
     * Register eventfd
     * io_uring is capable of posting events on an eventfd instance whenever completions occur.
     * This capability enables processes that are multiplexing I/O using poll(2) or epoll(7)
     * to add a io_uring registered eventfd instance file descriptor to the interest list so
     * that poll(2) or epoll(7) can notify them when a completion via io_uring occurred.
     *
     * @returns None
     */

    os_fd_t RegisterEventFd();

    /**
     * Unregister eventfd
     *
     * @return None
     */
    void UnregisterEventFd();

    /**
     * Return internal io_uring handle
     *
     * @return io_uring handle
     */
    [[nodiscard]] io_uring *Ring() noexcept
    {
        return std::addressof(ring_);
    }

    /**
     * Handles the response from completion queue
     *
     * @param cqe completion queue entry
     * @return None
     */
    void HandleCqe(cqe_t *cqe);

    /**
     * Starts a new task in separate thread context which will submit request to submission queue
     * and process kernel response from completion queue.
     *
     */
    void ProcessIoRequests();
    Status DoProcessIoRequests();
    bool SubmitToKernelRing(std::unique_ptr<IoRequest> req, IoCompletion *completion);

    /**
     * Fetch the cqes from completion ring if any cqe is marked completed by kernel
     *
     */
    Status ProcessCompletionsRing();
    Status DoProcessComplitionsRing(size_t max_requests = 0);

    /**
     * Prepare SQE from `IoRequest` object
     *
     */
    Status PrepareSqe(const std::unique_ptr<IoRequest> &req, IoCompletion *completion);

    /**
     * Flush the prepared SQEs to kernel uring
     *
     */
    Status SubmitToKernel();

    void Noop(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Read(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void ReadFixed(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void ReadV(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Write(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void WriteFixed(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void WriteV(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Accept(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Connect(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Recv(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Send(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void SendZc(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void RecvMsg(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void SendMsg(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void SendMsgZc(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Fallocate(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Close(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Fsync(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Timeout(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void TimeoutRemove(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void LinkTimeout(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Openat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Statx(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Splice(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Tee(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Shutdown(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Renameat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Symlinkat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Linkat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Unlinkat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Makedirat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void PollAdd(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void PollRemove(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void EpollCtl(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void ProvideBuffer(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void RemoveBuffer(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Cancel(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void FileSyncRange(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void FilesUpdate(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Fadvise(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Madvise(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Openat2(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Msgring(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Fsetxattr(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Setxattr(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Fgetxattr(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Getxattr(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);
    void Socket(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion);

    Status LinkTimeoutTo(sqe_t *sqe, uint64_t timeout /*millisec*/);
    Status Nop();

    inline io_uring_sqe *GetSqe();
    inline void FetchAdd(std::atomic<ssize_t> &a, ssize_t b);
    inline void IncrementSubmissionCount(ssize_t b);
    inline void DecrementSubmissionCount(ssize_t b);

    /**
     * Prepare provided buffer
     */
    Status PrepareProvidedBufferFirst(io_buf_gid_t group_id, uint32_t buffer_size, uint32_t buffer_count);
    Status PreparePreApprovedProvidedBufferFirst(io_buf_gid_t group_id, char *bufs, uint32_t buffer_size, uint32_t buffer_count);
    Status SubmiteProvidedBufferFirst(void *bufs, io_buf_gid_t group_id, uint32_t buffer_size, uint32_t buffer_count);

private:
    IoUringParams params_external_;
    io_uring_params params_;
    io_uring ring_;
    os_fd_t event_fd_{IOURING_INVALID_SOCKET};
    bool probe_ops_[IORING_OP_LAST] = {};
    IoUringFileFdContainer registered_fds_;
    std::mutex mutex_;
    std::mutex files_mutex_;
    ThreadPool thread_pool_;
    IoSink io_sink_;
    bool stop_consumption_;
    std::atomic<ssize_t> submission_pending_count_ = 0;
    std::vector<io_uring_cqe *> cqes_;
    std::map<io_buf_gid_t /*gid*/, std::unique_ptr<BufferPool>> buffer_pools_;
    std::atomic<bool> linked_sqe_;

    IouringStats stats_;
};

/// Implementation
template<typename Completion>
auto IoUring::SubmitIoRequest(std::unique_ptr<IoRequest> req, Completion *completion)
{
    using promise_type = typename Completion::promise_type;
    IoFuture<promise_type> iofuture(completion->GetFuture(), (void *)completion);
    if (params_external_.start_sub_consume_thread_)
    {
        // Internal thread to submit/consume task is running. So enque the request
        // to io_sink.
        io_sink_.EnqueueIoRequest(std::move(req), completion);
    }
    else
    {
        if (IOURING_UNLIKELY((event_fd_ < 0)))
        {
            IOURING_ASSERT(false, "Event fd should be registered to use this API");
        }
        // io_uring_get_sqe() and till io_uring_submit() must be atomical.
        std::lock_guard<std::mutex> _(mutex_);
        auto ret = SubmitToKernelRing(std::move(req), completion);
        if (!ret)
        {
            try
            {
                throw std::runtime_error("Failed to submit to ring");
            }
            catch (...)
            {
                try
                {
                    completion->SetException(std::current_exception());
                    completion = nullptr;
                }
                catch (...)
                {}
            }
        }
    }
    return iofuture;
}

} // namespace iouring
