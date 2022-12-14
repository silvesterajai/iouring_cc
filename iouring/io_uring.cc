#include "iouring/io_uring.h"

#include "iouring/assert.h"         // for IOURING_ASSERT
#include "iouring/buffer_pool.h"    // for BufferPool
#include "iouring/defer.h"          // for DeferredAction
#include "iouring/io_completion.h"  // for DefaultCompletion, IoCompletion
#include "iouring/io_request.h"     // for IoRequest, IoRequest::Operation, IoReque...
#include "iouring/io_sink.h"        // for IoSink
#include "iouring/io_uring_utils.h" // for msec_to_timespec, IOURING_UNLIKELY, IOUR...
#include "iouring/status.h"         // for Status, StatusCode, StatusCode::kUnimple...
#include "iouring/threadpool.h"     // for ThreadPool
#include "logger.h"
#include <algorithm>     // for fill, min
#include <cstring>       // for memset, strerror
#include <cxxabi.h>      // for __forced_unwind
#include <errno.h>       // for EBUSY, EINTR, ETIME
#include <liburing.h>    // for io_uring_sqe_set_data, io_uring_sqe_set_...
#include <memory>        // for unique_ptr, allocator, make_unique, shar...
#include <sys/eventfd.h> // for eventfd, eventfd_read, EFD_NONBLOCK, eve...
#include <unistd.h>      // for close
#include <utility>       // for pair, addressof, move

namespace iouring {

bool IsIoUringSupported()
{
    struct io_uring_params p
    {};
    struct io_uring ring;

    bool is_supported = io_uring_queue_init_params(2, &ring, &p) == 0;
    if (is_supported)
    {
        io_uring_queue_exit(&ring);
    }

    return is_supported;
}
} // namespace iouring

using namespace iouring;

IoUringFileFdContainer::IoUringFileFdContainer(int max_fds)
    : max_size_(max_fds)
    , free_ids_(max_fds, INVALID_ID)
{}

int IoUringFileFdContainer::LookUp(int fd)
{
    auto it = fd_to_index_.find(fd);
    if (it != fd_to_index_.end())
        return it->second;
    return -1;
}

void IoUringFileFdContainer::Remove(int fd)
{
    auto it = fd_to_index_.find(fd);
    if (it == fd_to_index_.end())
        return;
    free_ids_[it->second] = INVALID_ID;
    fd_to_index_.erase(it);
}

int IoUringFileFdContainer::Add(int fd)
{
    auto it = fd_to_index_.find(fd);
    if (it != fd_to_index_.end())
        return it->second;
    int index = 0;
    for (; index < max_size_; ++index)
    {
        if (free_ids_[index] == INVALID_ID)
        {
            free_ids_[index] = fd;
            break;
        }
    }
    fd_to_index_[fd] = index;
    return index;
}

void IoUringFileFdContainer::CleanUp()
{
    fd_to_index_.clear();
    free_ids_.clear();
    free_ids_.resize(max_size_, INVALID_ID);
}

IoUring::IoUring(const IoUringParams &params, uint32_t wq_fd)
    : params_external_(params)
    , registered_fds_(params.max_fd_slots_)
    , thread_pool_(1)
    , stop_consumption_(false)
    , cqes_(params_external_.max_cqe_to_drain_, nullptr)
    , linked_sqe_(false)
{
    memset(&ring_, 0, sizeof(ring_));
    memset(&params_, 0, sizeof(params_));
    memset(probe_ops_, 0, sizeof(probe_ops_));

    params_.flags = params.flag_;
    params_.wq_fd = wq_fd;

    auto ret = io_uring_queue_init_params(params_external_.cq_entries_, &ring_, &params_);
    if (IOURING_UNLIKELY((ret < 0)))
    {
        IOURING_ASSERT(false, "Failed to init io_uring");
    }

    auto *probe = io_uring_get_probe_ring(&ring_);
    IOURING_ASSERT(probe, "Failed to probe ring");

    DeferredAction free_probe([=]() { io_uring_free_probe(probe); });
    auto probe_fn = [=](uint32_t opcode) {
        for (int i = 0; i < probe->ops_len; ++i)
        {
            if (probe->ops[i].op == opcode && probe->ops[i].flags & IO_URING_OP_SUPPORTED)
            {
                probe_ops_[i] = true;
                break;
            }
        }
    };
    for (uint32_t i = 0; i < IORING_OP_LAST; ++i)
    {
        probe_fn(i);
        LOG_TRACE("{:#x} is {}", i, probe_ops_[i] ? "supported" : "not supported");
    }

    if (registered_fds_.Capacity())
    {
        std::vector<int> tmp;
        tmp.resize(registered_fds_.Capacity(), IoUringFileFdContainer::INVALID_ID);
        ret = io_uring_register_files(&ring_, tmp.data(), registered_fds_.Capacity());
        IOURING_ASSERT(ret == 0, fmt::format("io_uring_register_files failed {}", ret));
    }

    if (!FeaturesSupported(params_external_.features_needed_))
    {
        IOURING_ASSERT(false, fmt::format("All required features are not supported by kernel, {:#x}", params_external_.features_needed_));
    }

    if (!OpsSupported(params_external_.required_ops_))
    {
        IOURING_ASSERT(false, "All required ops are not supported by kernel");
    }

    for (auto &[key, val] : params_external_.pre_allocate_provided_buffers_)
    {
        auto ret = PreparePreApprovedProvidedBufferFirst(key, val->Data(), val->Size(), val->Count());
        if (!ret.Ok())
        {
            IOURING_ASSERT(false, "Failed to prepare provied buffers");
        }
    }
    for (auto &[key, val] : params_external_.provided_buffers_)
    {
        auto ret = PrepareProvidedBufferFirst(key, val.first, val.second);
        if (!ret.Ok())
        {
            IOURING_ASSERT(false, "Failed to prepare provied buffers");
        }
    }

    if (params_external_.register_event_fd_)
    {
        RegisterEventFd();
    }

    if (params_external_.registered_buffers_.first)
    {
        RegisterBuffers(params_external_.registered_buffers_.first, params_external_.registered_buffers_.second);
    }

    if (params_external_.start_sub_consume_thread_)
    {
        // Starts a new task in separate thread context which will submit request to submission queue
        // and process kernel response from completion queue.
        ProcessIoRequests();
    }
}

IoUring::~IoUring()
{
    io_sink_.Stop();
    stop_consumption_ = true;
    if (event_fd_ > 0)
    {
        close(event_fd_);
        event_fd_ = IOURING_INVALID_SOCKET;
    }
    thread_pool_.Join();
    io_uring_queue_exit(&ring_);
    registered_fds_.CleanUp();
}

os_fd_t IoUring::RegisterEventFd()
{
    IOURING_ASSERT(!IsEventFdRegistered(), "Event fd is already registered");
    event_fd_ = eventfd(0, EFD_NONBLOCK);
    [[maybe_unused]] int res = io_uring_register_eventfd(&ring_, event_fd_);
    IOURING_ASSERT(res == 0, "Unable to register eventfd");
    IOURING_ASSERT(event_fd_ > 0, "Event fd is not valid");
    return event_fd_;
}

bool IoUring::IsEventFdRegistered()
{
    return (event_fd_ != IOURING_INVALID_SOCKET);
}

bool IoUring::OpsSupported(std::vector<io_uring_op> required_ops)
{
    auto *probe = io_uring_get_probe_ring(&ring_);
    DeferredAction free_probe([=]() { io_uring_free_probe(probe); });

    for (auto op : required_ops)
    {
        if (!io_uring_opcode_supported(probe, op))
        {
            LOG_ERROR("OP code {:#x} is not supported", op);
            return false;
        }
    }
    return true;
}

bool IoUring::FeaturesSupported(int required_features)
{
    LOG_DEBUG("Ring features {:#x}", ring_.features);
    if (~ring_.features & required_features)
        return false;
    return true;
}

void IoUring::UnregisterEventFd()
{
    [[maybe_unused]] int res = io_uring_unregister_eventfd(&ring_);
    IOURING_ASSERT(res == 0, "Unable to unregister eventfd");
    event_fd_ = IOURING_INVALID_SOCKET;
}

Status IoUring::RegisterFile(int fd)
{
    std::lock_guard<std::mutex> _(files_mutex_);
    auto index = registered_fds_.LookUp(fd);
    if (index != IoUringFileFdContainer::INVALID_ID)
        return Status::AlreadyExistsError(fmt::format("fd {} is already registered", fd));
    if (registered_fds_.Capacity() == registered_fds_.Size())
        return Status::ResourceExhaustedError("No free index available");
    index = registered_fds_.Add(fd);
    auto ret = io_uring_register_files_update(&ring_, (uint32_t)index, &fd, 1);
    if (ret < 0)
        return Status::ErrnoToStatus(-ret, "Failed to register files to io_uring");
    return Status::OkStatus();
}

Status IoUring::RegisterFiles(std::vector<fd_t> &fds)
{
    Status res = Status::OkStatus();
    int index = 0;
    bool unregister = false;
    for( auto &fd : fds)
    {
        res = RegisterFile(fd);
        if (!res.Ok())
        {
            unregister = true;
            break;
        }
        ++index;
    }
    if (unregister)
    {
        for(int i = 0; i < index; ++i)
        {
            auto res_unreg = UnregisterFile(fds[i]);
        }
    }
    return res;
}

int IoUring::RegisterFilesExternal(std::vector<fd_t> &fds, int index, bool is_update)
{
    if (is_update)
    {
        // Returns number of files updated on success, -ERROR on failure.
        return io_uring_register_files_update(&ring_, index, fds.data(), fds.size());
    }
    // Returns 0 on success and -errono on failure.
    auto result = io_uring_register_files(&ring_, fds.data(), fds.size());
    if (result == 0)
        return fds.size();
    return result;
}

Status IoUring::UnregisterFile(int fd)
{
    std::lock_guard<std::mutex> _(files_mutex_);
    if (registered_fds_.Size() == 0)
    {
        return UnregisterFiles();
    }
    auto index = registered_fds_.LookUp(fd);
    if (index == IoUringFileFdContainer::INVALID_ID)
        return Status::NotFoundError(fmt::format("fd {} is not registered", fd));
    registered_fds_.Remove(fd);
    int tmp_fd = -1;
    auto ret = io_uring_register_files_update(&ring_, index, &tmp_fd, 1);
    if (ret < 0)
        return Status::ErrnoToStatus(-ret, "Failed to register files to io_uring");
    return Status::OkStatus();
}

Status IoUring::UnregisterFiles()
{
    auto ret = io_uring_unregister_files(&ring_);
    if (ret < 0)
        return Status::ErrnoToStatus(-ret, "Failed to unregister files from io_uring");
    registered_fds_.CleanUp();
    return Status::OkStatus();
}

void IoUring::RegisterBuffers(const struct iovec *iovecs, uint32_t nr_iovecs)
{
    io_uring_register_buffers(&ring_, iovecs, nr_iovecs) | panic_on_err("io_uring_register_buffers", false);
}

void IoUring::UnregisterBuffers()
{
    io_uring_unregister_buffers(&ring_);
}

Status IoUring::SubmiteProvidedBufferFirst(void *bufs, io_buf_gid_t group_id, uint32_t buffer_size, uint32_t buffer_count)
{
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;

    sqe = GetSqe();
    io_uring_prep_provide_buffers(sqe, bufs, buffer_size, buffer_count, group_id, 0 /*bid*/);
    LOG_DEBUG("io_uring_prep_provide_buffers. buffer_size:{}, buffer_count:{}, gid:{}, bid:0", buffer_size, buffer_count, group_id);

    io_uring_submit(&ring_);
    io_uring_wait_cqe(&ring_, &cqe);
    auto ret = cqe->res;
    io_uring_cqe_seen(&ring_, cqe);
    if (ret < 0)
    {
        LOG_ERROR("PrepareProvidedBuffer failed with error {}", ret);
        return Status::InternalError("PrepareProvidedBuffer failed");
    }
    return Status::OkStatus();
}

Status IoUring::PreparePreApprovedProvidedBufferFirst(io_buf_gid_t group_id, char *bufs, uint32_t buffer_size, uint32_t buffer_count)
{
    auto it = buffer_pools_.find(group_id);
    if (it != buffer_pools_.end())
    {
        return Status::AlreadyExistsError(fmt::format("{} group_id buffer pool already exists"));
    }
    IOURING_ASSERT(buffer_count > 0, "buffer count should be greater than zero");
    IOURING_ASSERT(buffer_size > 0, "buffer size should be greater than zero");
    LOG_DEBUG("Prepare provided buffer. gid:{}, buffer_size:{}, buffer_count:{}", group_id, buffer_size, buffer_count);
    buffer_pools_.emplace(group_id, std::make_unique<BufferPool>(buffer_size, buffer_count, bufs));

    // register buffers for buffer selection
    auto &buffer = buffer_pools_[group_id];
    return SubmiteProvidedBufferFirst(buffer->Data(), group_id, buffer->Size(), buffer->Count());
}

Status IoUring::PrepareProvidedBufferFirst(io_buf_gid_t group_id, uint32_t buffer_size, uint32_t buffer_count)
{
    auto it = buffer_pools_.find(group_id);
    if (it != buffer_pools_.end())
    {
        return Status::AlreadyExistsError(fmt::format("{} group_id buffer pool already exists"));
    }
    IOURING_ASSERT(buffer_count > 0, "buffer count should be greater than zero");
    IOURING_ASSERT(buffer_size > 0, "buffer size should be greater than zero");
    LOG_DEBUG("Prepare provided buffer. gid:{}, buffer_size:{}, buffer_count:{}", group_id, buffer_size, buffer_count);
    buffer_pools_.emplace(group_id, std::make_unique<BufferPool>(buffer_size, buffer_count));

    // register buffers for buffer selection
    auto &buffer = buffer_pools_[group_id];
    return SubmiteProvidedBufferFirst(buffer->Data(), group_id, buffer->Size(), buffer->Count());
}

/// Returns a pointer to a vacant SQE.
/// If the submission queue is full, then process submission queue and completion queue for `max_try`, untill a vacant SQE is available. If
/// vacant SQE is not available after `max_try` return nullptr. Calling function should perform error handling.
io_uring_sqe *IoUring::GetSqe()
{
    static int max_try = 10;
    int current_count = 0;
    io_uring_sqe *sqe;
    while (IOURING_UNLIKELY((sqe = io_uring_get_sqe(&ring_)) == nullptr))
    {
        auto ret = SubmitToKernel();
        ret = ProcessCompletionsRing();
        ++current_count;
        if (current_count >= max_try)
            return sqe;
    }
    return sqe;
}

Status IoUring::LinkTimeoutTo(sqe_t *sqe, uint64_t timeout /*millisec*/)
{
    sqe->flags |= IOSQE_IO_LINK;

    __kernel_timespec ts;
    msec_to_timespec(timeout, ts);
    auto timeout_sqe = GetSqe();
    if (!timeout_sqe)
    {
        LOG_ERROR("Not able to get free SQE element from uring.");
        return Status::ResourceExhaustedError("Not able to get free SQE element from uring");
    }
    io_uring_prep_link_timeout(timeout_sqe, &ts, 0);
    return Status::OkStatus();
}

Status IoUring::Nop()
{
    auto sqe = GetSqe();
    if (!sqe)
    {
        LOG_ERROR("Not able to get free SQE element from uring.");
        return Status::ResourceExhaustedError("Not able to get free SQE element from uring");
    }
    auto comp = std::make_unique<DefaultCompletion>();
    io_uring_prep_nop(sqe);
    io_uring_sqe_set_data(sqe, comp.release());
    return Status::OkStatus();
}

void IoUring::Noop(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    [[maybe_unused]] const auto &op = req->As<IoRequest::Operation::IORING_OP_NOP>();
    io_uring_prep_nop(sqe);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Read(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_READ>();

    // Check if the fd is pre-registered fd
    if (auto new_fd = registered_fds_.LookUp(op.fd); new_fd != IoUringFileFdContainer::INVALID_ID)
    {
        io_uring_prep_read(sqe, new_fd, op.addr, op.size, op.offset);
        req->flags_.sqe_flag |= IOSQE_FIXED_FILE; 
    }
    else
    {
        io_uring_prep_read(sqe, op.fd, op.addr, op.size, op.offset);
    }
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
    sqe->buf_group = op.buf_gid;
}

void IoUring::ReadFixed(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_READ_FIXED>();
    io_uring_prep_read_fixed(sqe, op.fd, op.addr, op.size, op.offset, op.index);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::ReadV(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_READV>();
    if (op.rw_flags)
        io_uring_prep_readv2(sqe, op.fd, op.iov, op.iov_len, op.offset, op.rw_flags);
    else
        io_uring_prep_readv(sqe, op.fd, op.iov, op.iov_len, op.offset);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Write(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_WRITE>();
    // Check if the fd is pre-registered fd
    if (auto new_fd = registered_fds_.LookUp(op.fd); new_fd != IoUringFileFdContainer::INVALID_ID)
    {
        io_uring_prep_write(sqe, new_fd, op.addr, op.size, op.offset);
        req->flags_.sqe_flag |= IOSQE_FIXED_FILE; 
    }
    else
    {
        io_uring_prep_write(sqe, op.fd, op.addr, op.size, op.offset);
    }
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::WriteFixed(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_WRITE_FIXED>();
    io_uring_prep_write_fixed(sqe, op.fd, op.addr, op.size, op.offset, op.index);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::WriteV(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_WRITEV>();
    if (op.rw_flags)
        io_uring_prep_writev2(sqe, op.fd, op.iov, op.iov_len, op.offset, op.rw_flags);
    else
        io_uring_prep_writev(sqe, op.fd, op.iov, op.iov_len, op.offset);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Accept(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_ACCEPT>();
    if (!op.multishot && (op.file_index == IOURING_INVALID_FILEINDEX))
    {
        io_uring_prep_accept(sqe, op.fd, op.sockaddr, op.socklen, op.flags);
    }
    else if (op.multishot && (op.file_index == IOURING_INVALID_FILEINDEX))
    {
        io_uring_prep_multishot_accept(sqe, op.fd, op.sockaddr, op.socklen, op.flags);
    }
    else if (!op.multishot && (op.file_index != IOURING_INVALID_FILEINDEX))
    {
        io_uring_prep_accept_direct(sqe, op.fd, op.sockaddr, op.socklen, op.flags, op.file_index);
    }
    else if (op.multishot && (op.file_index != IOURING_INVALID_FILEINDEX))
    {
        io_uring_prep_multishot_accept_direct(sqe, op.fd, op.sockaddr, op.socklen, op.flags);
    }

    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Connect(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_CONNECT>();
    io_uring_prep_connect(sqe, op.fd, op.sockaddr, op.socklen);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Recv(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_RECV>();
    io_uring_prep_recv(sqe, op.fd, op.addr, op.size, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
    sqe->buf_group = op.buf_gid;
}

void IoUring::Send(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SEND>();
    io_uring_prep_send(sqe, op.fd, op.addr, op.size, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::SendZc(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SEND_ZC>();
    if (op.file_index != IOURING_INVALID_FILEINDEX)
        io_uring_prep_send_zc(sqe, op.fd, op.addr, op.size, op.flags, op.zc_flags);
    else
        io_uring_prep_send_zc_fixed(sqe, op.fd, op.addr, op.size, op.flags, op.zc_flags, op.file_index);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::RecvMsg(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_RECVMSG>();
#if 0
    if (op.multishot)
    {
        io_uring_prep_recvmsg_multishot(sqe, op.fd, op.msghdr, op.flags);
        // The multishot version requires the IOSQE_BUFFER_SELECT flag to be set
        // IOSQE_BUFFER_SELECT flag should always have associated buffer group id.
        sqe->buf_group = op.buf_gid; 
    }
    else
#endif
    {
        io_uring_prep_recvmsg(sqe, op.fd, op.msghdr, op.flags);
    }
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::SendMsg(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SENDMSG>();
    io_uring_prep_sendmsg(sqe, op.fd, op.msghdr, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::SendMsgZc(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SENDMSG_ZC>();
    io_uring_prep_sendmsg_zc(sqe, op.fd, op.msghdr, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Fallocate(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_FALLOCATE>();
    io_uring_prep_fallocate(sqe, op.fd, op.mode, op.offset, op.len);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Close(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_CLOSE>();
    if (op.fd > 0)
        io_uring_prep_close(sqe, op.fd);
    else if (op.file_index != IOURING_INVALID_FILEINDEX)
        io_uring_prep_close_direct(sqe, op.file_index);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Fsync(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_FSYNC>();
    io_uring_prep_fsync(sqe, op.fd, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Timeout(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_TIMEOUT>();
    __kernel_timespec ts;
    ts.tv_nsec = op.timeout.count();
    io_uring_prep_timeout(sqe, &ts, op.count, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::TimeoutRemove(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_TIMEOUT_REMOVE>();
    if (op.user_data && (op.timeout > std::chrono::nanoseconds(0)))
    {
        __kernel_timespec ts;
        ts.tv_nsec = op.timeout.count();
        io_uring_prep_timeout_update(sqe, &ts, op.user_data, op.flags);
    }
    else
    {
        io_uring_prep_timeout_remove(sqe, op.user_data, op.flags);
    }
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::LinkTimeout(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_LINK_TIMEOUT>();
    __kernel_timespec ts;
    chrono_to_kts(op.timeout, ts);
    io_uring_prep_link_timeout(sqe, &ts, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Openat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_OPENAT>();
    if (op.file_index == IOURING_INVALID_FILEINDEX)
        io_uring_prep_openat(sqe, op.dirfd, op.path_name, op.flags, op.mode);
    else
        io_uring_prep_openat_direct(sqe, op.dirfd, op.path_name, op.flags, op.mode, op.file_index);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Statx(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_STATX>();
    io_uring_prep_statx(sqe, op.dirfd, op.path_name, op.flags, op.mask, op.statxbuf);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Splice(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SPLICE>();
    io_uring_prep_splice(sqe, op.in_fd, op.in_offset, op.out_fd, op.out_offset, op.size, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Tee(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_TEE>();
    io_uring_prep_tee(sqe, op.in_fd, op.out_fd, op.size, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Shutdown(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SHUTDOWN>();
    io_uring_prep_shutdown(sqe, op.fd, op.how);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Renameat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_RENAMEAT>();
    if (op.olddirfd == -1 || op.newdirfd == -1)
        io_uring_prep_rename(sqe, op.oldpath, op.newpath);
    else
        io_uring_prep_renameat(sqe, op.olddirfd, op.oldpath, op.newdirfd, op.newpath, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Symlinkat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SYMLINKAT>();
    if (op.newdirfd == -1)
        io_uring_prep_symlink(sqe, op.target, op.linkpath);
    else
        io_uring_prep_symlinkat(sqe, op.target, op.newdirfd, op.linkpath);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Linkat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_LINKAT>();
    if (op.olddirfd == -1 || op.newdirfd == -1)
        io_uring_prep_link(sqe, op.oldpath, op.newpath, op.flags);
    io_uring_prep_linkat(sqe, op.olddirfd, op.oldpath, op.newdirfd, op.newpath, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Unlinkat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_UNLINKAT>();
    if (op.dirfd == -1)
        io_uring_prep_unlink(sqe, op.path, op.flags);
    else
        io_uring_prep_unlinkat(sqe, op.dirfd, op.path, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Makedirat(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_MKDIRAT>();
    if (op.dirfd == -1)
        io_uring_prep_mkdir(sqe, op.path, op.mode);
    else
        io_uring_prep_mkdirat(sqe, op.dirfd, op.path, op.mode);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::PollAdd(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_POLL_ADD>();
    if (op.multishot)
        io_uring_prep_poll_multishot(sqe, op.fd, op.events);
    else
        io_uring_prep_poll_add(sqe, op.fd, op.events);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::PollRemove(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_POLL_REMOVE>();
    if (op.flags)
    {
        io_uring_prep_poll_update(sqe, op.user_data, op.new_user_data, op.events, op.flags);
    }
    else
    {
        io_uring_prep_poll_remove(sqe, op.user_data);
    }
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::FileSyncRange(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SYNC_FILE_RANGE>();
    io_uring_prep_sync_file_range(sqe, op.fd, op.len, op.offset, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::FilesUpdate(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_FILES_UPDATE>();
    io_uring_prep_files_update(sqe, op.fds, op.fds_len, op.offset);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::EpollCtl(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_EPOLL_CTL>();
    io_uring_prep_epoll_ctl(sqe, op.epfd, op.fd, op.op, op.event);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::ProvideBuffer(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_PROVIDE_BUFFERS>();
    auto msg_len = op.len;
    if (op.len == 0)
        msg_len = buffer_pools_.find(op.gid)->second->Size();
    io_uring_prep_provide_buffers(sqe, op.buffer, msg_len, op.nbufs, op.gid, op.bid);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::RemoveBuffer(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_REMOVE_BUFFERS>();
    auto it = buffer_pools_.find(op.gid);
    IOURING_ASSERT((it != buffer_pools_.end()), fmt::format("{} group id is not found in registered buffers", op.gid));
    auto nbufs = op.nbufs;
    if (nbufs == 0)
        nbufs = it->second->Count();
    io_uring_prep_remove_buffers(sqe, nbufs, op.gid);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
    if (nbufs == 0)
    {
        auto ret = EraseBufferFromPool(op.gid);
    }
}

void IoUring::Cancel(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_ASYNC_CANCEL>();
    if (op.user_data)
        io_uring_prep_cancel(sqe, op.user_data, op.flags);
    else if (op.user_data_64)
        io_uring_prep_cancel64(sqe, op.user_data_64, op.flags);
    else if (op.fd != IOURING_INVALID_SOCKET)
        io_uring_prep_cancel_fd(sqe, op.fd, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Fadvise(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_FADVISE>();
    io_uring_prep_fadvise(sqe, op.fd, op.offset, op.len, op.advice);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Madvise(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_MADVISE>();
    io_uring_prep_madvise(sqe, op.addr, op.len, op.advice);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Openat2(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_OPENAT2>();
    if (op.file_index != IOURING_INVALID_FILEINDEX)
    {
#if 0
        io_uring_prep_openat2(sqe, op.dirfd, op.path_name, op.flags, op.how);
#endif
        io_uring_prep_openat2(sqe, op.dirfd, op.path_name, op.how);
    }
    else
    {
#if 0
        io_uring_prep_openat2_direct(sqe, op.dirfd, op.path_name, op.flags, op.how, op.file_index);
#endif
        io_uring_prep_openat2_direct(sqe, op.dirfd, op.path_name, op.how, op.file_index);
    }
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Fsetxattr(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_FSETXATTR>();
    io_uring_prep_fsetxattr(sqe, op.fd, op.name, op.value, op.flags, op.len);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Setxattr(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SETXATTR>();
    io_uring_prep_setxattr(sqe, op.name, op.value, op.path, op.flags, op.len);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Fgetxattr(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_FGETXATTR>();
    io_uring_prep_fgetxattr(sqe, op.fd, op.name, (char*)op.value, op.len);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Getxattr(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_GETXATTR>();
    io_uring_prep_getxattr(sqe, op.name, (char*)op.value, op.path, op.len);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Msgring(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_MSG_RING>();
    io_uring_prep_msg_ring(sqe, op.fd, op.data_32, op.data_64, op.flags);
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

void IoUring::Socket(sqe_t *sqe, const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    const auto &op = req->As<IoRequest::Operation::IORING_OP_SOCKET>();
    if (op.file_index == IOURING_INVALID_FILEINDEX)
    {
        if (!op.direct_alloc)
            io_uring_prep_socket(sqe, op.domain, op.type, op.protocol, op.flags);
        else
            io_uring_prep_socket_direct_alloc(sqe, op.domain, op.type, op.protocol, op.flags);
    }
    else
    {
        io_uring_prep_socket_direct(sqe, op.domain, op.type, op.protocol, op.file_index, op.flags);
    }
    io_uring_sqe_set_data(sqe, completion);
    io_uring_sqe_set_flags(sqe, req->flags_.sqe_flag);
}

inline void IoUring::FetchAdd(std::atomic<ssize_t> &a, ssize_t b)
{
    a.fetch_add(b, std::memory_order_relaxed);
}

void IoUring::IncrementSubmissionCount(ssize_t b)
{
    FetchAdd(submission_pending_count_, b);
}

void IoUring::DecrementSubmissionCount(ssize_t b)
{
    FetchAdd(submission_pending_count_, -b);
}

Status IoUring::FlushSubmissionRing()
{
    std::lock_guard<std::mutex> _(mutex_);
    return SubmitToKernel();
}

Status IoUring::SubmitToKernel()
{
    int res = -1;
    do
    {
        res = io_uring_submit(&ring_);
    } while (res == -EINTR);

    IOURING_ASSERT((res >= 0 || res == -EBUSY), "unable to submit io_uring queue entries");
    if (res < 0)
    {
        return Status::ErrnoToStatus(-res, "Failure to submit io_uring, busy");
    }
    IncrementSubmissionCount(res);
    LOG_DEBUG("SubmitToKernel, count: {}", res);
    return Status::OkStatus();
}

Status IoUring::EraseBufferFromPool(io_buf_gid_t gid)
{
    auto it = buffer_pools_.find(gid);
    if (IOURING_UNLIKELY(it == buffer_pools_.end()))
    {
        return Status::NotFoundError(fmt::format("{} gid is not present in buffer pool", gid));
    }
    buffer_pools_.erase(it);
    return Status::OkStatus();
}

Status IoUring::PrepareSqe(const std::unique_ptr<IoRequest> &req, IoCompletion *completion)
{
    using Operation = IoRequest::Operation;
    auto sqe = GetSqe();
    if (!sqe)
    {
        LOG_ERROR("Not able to get free SQE element from uring.");
        return Status::ResourceExhaustedError("Not able to get free SQE element from uring");
    }
    LOG_DEBUG("PrepareSqe, opcode: {}, handler: {}.", IoRequest::OperationStr(req->OpCode()), (void *)completion);
    switch (req->OpCode())
    {
    case Operation::IORING_OP_NOP:
        Noop(sqe, req, completion);
        break;
    case Operation::IORING_OP_READ:
        Read(sqe, req, completion);
        break;
    case Operation::IORING_OP_READV:
        ReadV(sqe, req, completion);
        break;
    case Operation::IORING_OP_WRITE:
        Write(sqe, req, completion);
        break;
    case Operation::IORING_OP_WRITEV:
        WriteV(sqe, req, completion);
        break;
    case Operation::IORING_OP_ACCEPT:
        Accept(sqe, req, completion);
        break;
    case Operation::IORING_OP_CONNECT:
        Connect(sqe, req, completion);
        break;
    case Operation::IORING_OP_RECV:
        Recv(sqe, req, completion);
        break;
    case Operation::IORING_OP_SEND:
        Send(sqe, req, completion);
        break;
    case Operation::IORING_OP_RECVMSG:
        RecvMsg(sqe, req, completion);
        break;
    case Operation::IORING_OP_SENDMSG:
        SendMsg(sqe, req, completion);
        break;
    case Operation::IORING_OP_FALLOCATE:
        Fallocate(sqe, req, completion);
        break;
    case Operation::IORING_OP_CLOSE:
        Close(sqe, req, completion);
        break;
    case Operation::IORING_OP_FSYNC:
        Fsync(sqe, req, completion);
        break;
    case Operation::IORING_OP_POLL_ADD:
        PollAdd(sqe, req, completion);
        break;
    case Operation::IORING_OP_POLL_REMOVE:
        PollRemove(sqe, req, completion);
        break;
    case Operation::IORING_OP_READ_FIXED:
        ReadFixed(sqe, req, completion);
        break;
    case Operation::IORING_OP_WRITE_FIXED:
        WriteFixed(sqe, req, completion);
        break;
    case Operation::IORING_OP_TIMEOUT:
        Timeout(sqe, req, completion);
        break;
    case Operation::IORING_OP_OPENAT:
        Openat(sqe, req, completion);
        break;
    case Operation::IORING_OP_STATX:
        Statx(sqe, req, completion);
        break;
    case Operation::IORING_OP_SPLICE:
        Splice(sqe, req, completion);
        break;
    case Operation::IORING_OP_TEE:
        Tee(sqe, req, completion);
        break;
    case Operation::IORING_OP_SHUTDOWN:
        Shutdown(sqe, req, completion);
        break;
    case Operation::IORING_OP_RENAMEAT:
        Renameat(sqe, req, completion);
        break;
    case Operation::IORING_OP_SYMLINKAT:
        Symlinkat(sqe, req, completion);
        break;
    case Operation::IORING_OP_LINKAT:
        Linkat(sqe, req, completion);
        break;
    case Operation::IORING_OP_UNLINKAT:
        Unlinkat(sqe, req, completion);
        break;
    case Operation::IORING_OP_MKDIRAT:
        Makedirat(sqe, req, completion);
        break;
    case Operation::IORING_OP_EPOLL_CTL:
        EpollCtl(sqe, req, completion);
        break;
    case Operation::IORING_OP_PROVIDE_BUFFERS:
        ProvideBuffer(sqe, req, completion);
        break;
    case Operation::IORING_OP_REMOVE_BUFFERS:
        RemoveBuffer(sqe, req, completion);
        break;
    case Operation::IORING_OP_ASYNC_CANCEL:
        Cancel(sqe, req, completion);
        break;
    case Operation::IORING_OP_LINK_TIMEOUT:
        LinkTimeout(sqe, req, completion);
        break;
    case Operation::IORING_OP_SYNC_FILE_RANGE:
        FileSyncRange(sqe, req, completion);
        break;
    case Operation::IORING_OP_FILES_UPDATE:
        FilesUpdate(sqe, req, completion);
        break;
    case Operation::IORING_OP_FADVISE:
        Fadvise(sqe, req, completion);
        break;
    case Operation::IORING_OP_MADVISE:
        Madvise(sqe, req, completion);
        break;
    case Operation::IORING_OP_OPENAT2:
        Openat2(sqe, req, completion);
        break;
    case Operation::IORING_OP_MSG_RING:
        Msgring(sqe, req, completion);
        break;
    case Operation::IORING_OP_FSETXATTR:
        Fsetxattr(sqe, req, completion);
        break;
    case Operation::IORING_OP_SETXATTR:
        Setxattr(sqe, req, completion);
        break;
    case Operation::IORING_OP_FGETXATTR:
        Fgetxattr(sqe, req, completion);
        break;
    case Operation::IORING_OP_GETXATTR:
        Getxattr(sqe, req, completion);
        break;
    case Operation::IORING_OP_SOCKET:
        Socket(sqe, req, completion);
        break;
    case Operation::IORING_OP_SEND_ZC:
        SendZc(sqe, req, completion);
        break;
    case Operation::IORING_OP_SENDMSG_ZC:
        SendMsgZc(sqe, req, completion);
        break;
    default:
        IOURING_ASSERT("{}, Unsupported operation", IoRequest::OperationStr(req->OpCode()));
        return Status::UnimplementedError("Operation is not implemented");
    }

    linked_sqe_ = (sqe->flags & (IOSQE_IO_LINK | IOSQE_IO_HARDLINK));
#ifdef IOURING_STATS
    stats_.UpdateSqeStats(sqe);
#endif
    return Status::OkStatus();
}

void IoUring::HandleCqe(cqe_t *cqe)
{
    IOURING_ASSERT(cqe, "cqe canot be null");
    if (cqe->user_data == LIBURING_UDATA_TIMEOUT)
    {
        LOG_DEBUG("HandleCqe, handler: {}, res: {}, flag: {:#x}", (void *)cqe->user_data, cqe->res, cqe->flags);
        return;
    }
    DecrementSubmissionCount(1);
    auto handler = static_cast<IoCompletion *>(io_uring_cqe_get_data(cqe));
    if (!handler)
        return;
    LOG_DEBUG("HandleCqe, handler: {}, res: {}, flag: {:#x}", (void *)handler, cqe->res, cqe->flags);
    if (handler->Gid() != IOURING_INVALID_BUFF_GID)
    {
        auto it = buffer_pools_.find(handler->Gid());
        if (IOURING_UNLIKELY(it == buffer_pools_.end()))
        {
            IOURING_ASSERT(false, fmt::format("Gid {} does not have corresponding buffer in buffer pool", handler->Gid()));
        }
        if (IOURING_LIKELY(cqe->flags & IORING_CQE_F_BUFFER))
        {
            // IORING_CQE_F_BUFFER	If set, the upper 16 bits are the buffer ID
            auto bid = cqe->flags >> 16;
            auto *buffer = it->second->Get(bid);
            handler->CompleteWith(cqe, buffer);
        }
    }
    else
    {
        handler->CompleteWith(cqe);
    }
}

Status IoUring::ProcessComplitionsRing()
{
    IOURING_ASSERT((event_fd_ > 0), "Event fd should be registered to use this API");
    IOURING_ASSERT((params_external_.start_sub_consume_thread_ == false),
                   "Cannot use this API if internal thread for sub/consume is enabled");
    std::lock_guard<std::mutex> _(mutex_);
    if (event_fd_ >= 0)
    {
        eventfd_t v;
        int ret = eventfd_read(event_fd_, &v);
        if (ret < 0)
        {
            LOG_ERROR("Failed to read data from event_fd_");
            return Status::InternalError("Failed to read data from event_fd_");
        }
    }
    return DoProcessComplitionsRing();

    // struct io_uring_cqe *cqe = nullptr;
    // int count = 0;
    // while (true)
    // {
    //     cqe = nullptr;
    //     int ret = io_uring_peek_cqe(&ring_, &cqe);
    //     if (ret != 0)
    //     {
    //         if (ret == -EAGAIN)
    //             break;
    //         else
    //             IOURING_ASSERT(false, fmt::format("io_uring_peek_cqe failed: {} {}", ret, strerror(ret)));
    //     }
    //     HandleCqe(cqe);
    //     io_uring_cqe_seen(&ring_, cqe);
    //     count++;
    // }

    // cqe_t *cqe;
    // unsigned num_cqes = 0;
    // unsigned head;

    // uint32_t nr_wait = 1;
    // int ret = io_uring_wait_cqes(&ring_, &cqe, nr_wait, &params_external_.cqe_wait_timeout_, nullptr);
    // if (IOURING_UNLIKELY(ret < 0))
    //     return Status::InternalError("");

    // io_uring_for_each_cqe(&ring_, head, cqe)
    // {
    //     HandleCqe(cqe);
    //     num_cqes++;
    // }

    // io_uring_cq_advance(&ring_, num_cqes);
    // return Status::OkStatus();
}

Status IoUring::DoProcessComplitionsRing(size_t max_requests)
{
    if (max_requests == 0)
    {
        max_requests = params_external_.max_cqe_to_drain_;
    }
    else if (max_requests > params_external_.max_cqe_to_drain_)
    {
        max_requests = params_external_.max_cqe_to_drain_;
    }

    struct io_uring_cqe *cqe = nullptr;
    auto ret = ::io_uring_wait_cqe_timeout(Ring(), &cqe, &params_external_.cqe_wait_timeout_);
    if ((ret < 0) && (ret != -ETIME && ret != -EBUSY))
    {
        LOG_ERROR("io_uring_wait_cqe_timeout, return is not success {}", ret);
        return Status(Status::ErrnoToStatusCode(ret), "io_uring_wait_cqe_timeout, return is not success", strerror(ret));
    }
    if (ret < 0)
    {
        LOG_TRACE("io_uring_wait_cqe_timeout, not cqes available {}", ret);
        return Status::OkStatus();
    }
    std::fill(cqes_.begin(), cqes_.end(), nullptr);
    // Returns number of ready cqes
    auto ready = ::io_uring_cq_ready(Ring());
    // Fills in an array of I/O completions up to count, if they are available, returning the count of completions filled
    auto n = ::io_uring_peek_batch_cqe(Ring(), cqes_.data(), std::min<int>(ready, max_requests));

    for (uint i = 0; i < n; ++i)
    {
        auto cqe = cqes_[i];
        HandleCqe(cqe);
    }
    ::io_uring_cq_advance(Ring(), n);
    return Status::OkStatus();
}

Status IoUring::ProcessCompletionsRing()
{

    // If there is not pending submission (i.e. SQE is submitted, but corresponding CQE is not procesed),
    // then return success without waiting for CQE from kernel
    if (submission_pending_count_ <= 0)
    {
        return Status::OkStatus();
    }

    // If stop_consumption_ flag is enabled, then skip processing of completion queue
    if (stop_consumption_)
    {
        LOG_WARN("Marked stop_consumption_ as false.");
        return Status::OkStatus();
    }
    else
    {
        return DoProcessComplitionsRing();
    }
}

bool IoUring::SubmitToKernelRing(std::unique_ptr<IoRequest> req, IoCompletion *completion)
{
    auto sqe_res = PrepareSqe(req, completion);
    if (sqe_res.IsUnimplemented())
    {
        // If io op is not implemented, then complete the request by setting below error code.
        // Calling function should handle the error response accordingly.
        cqe_t cqe;
        cqe.res = -1 * (int)StatusCode::kUnimplemented2;
        completion->CompleteWith(std::addressof(cqe));
        return true;
    }
    return !sqe_res.IsResourceExhausted();
}

Status IoUring::DoProcessIoRequests()
{
    static bool forever = true;
    do
    {
        // clang-format off
        // Returns number of SQEs prepared
        auto count = io_sink_.Drain([&](std::unique_ptr<IoRequest> req, IoCompletion *completion) -> bool
        {
            return SubmitToKernelRing(std::move(req), completion);
        }, params_external_.max_cqe_to_drain_, submission_pending_count_);
        // clang-format on

        if (count > 0 && !linked_sqe_)
        {
            auto flush_ret = SubmitToKernel();
        }

        // Drain returns count less than 0, if io_sink_ is stopped or some unrecoverable error
        // Break the loop in such errors
        if (count < 0)
            return Status::CancelledError("IoSink failed to drain reqs");

        auto cqe_res = ProcessCompletionsRing();
    } while (forever);
    return Status::OkStatus();
}

void IoUring::ProcessIoRequests()
{
    thread_pool_.enqueue([this] { [[maybe_unused]] auto ret = this->DoProcessIoRequests(); });
}


void IouringStats::UpdateSqeStats(const sqe_t *sqe)
{
    ops_[sqe->opcode]++;
    for(int i = 0; i < 8; ++i)
    {
        if(sqe->flags & (1U << i))
        {
            sqe_flags_[i]++;
        }
    }
}
void IouringStats::UpdateCqeStats(const cqe_t *cqe)
{
    (void)cqe;
}