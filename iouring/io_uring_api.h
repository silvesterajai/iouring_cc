#pragma once

#include "assert.h"
#include "iouring/io_completion.h"
#include "iouring/io_request.h"
#include "iouring/io_uring.h"
#include "iouring/io_uring_utils.h"
#include <liburing.h>
#include <linux/openat2.h>

namespace iouring {

/// @brief Do not perform any I/O operation.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Noop(IoUring &io_ring, const IoApiCommonVars &common_vars = IoApiCommonVars(), Func &&handler_fn = NoOpIoCompletionCb)
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_noop(common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Vectored read, equivalent to `preadv2(2)`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto ReadVector(IoUring &io_ring,
                fd_t fd,
                std::vector<struct iovec> &iovecs,
                off_t offset,
                flag_t rw_flags = 0,
                Func &&handler_fn = NoOpIoCompletionCb,
                const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_readv(fd, offset, iovecs, rw_flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Vectored write, equivalent to `pwritev2(2)`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto WriteVector(IoUring &io_ring,
                 fd_t fd,
                 std::vector<struct iovec> &iovecs,
                 off_t offset,
                 flag_t rw_flags = 0,
                 Func &&handler_fn = NoOpIoCompletionCb,
                 const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_writev(fd, offset, iovecs, rw_flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief File sync, equivalent to `fsync(2)`.
///
/// Note that, while I/O is initiated in the order in which it appears in the submission queue, completions are unordered. For example, an
/// application which places a write I/O followed by an fsync in the submission queue cannot expect the fsync to apply to the write. The two
/// operations execute in parallel, so the fsync may complete before the write is issued to the storage. The same is also true for
/// previously issued writes that have not completed prior to the fsync.
/// @see IOSQE_IO_LINK usage on how the ensure run fsync only after previously issues writes are
/// completed
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Fsync(IoUring &io_ring,
           fd_t fd,
           flag_t flags,
           Func &&handler_fn = NoOpIoCompletionCb,
           const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_fsync(fd, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Read request with a previously registered IO buffer that have been registered with io_uring_register_buffers.
/// @note The `buf` and `count` arguments must fall within a region specified by `buf_index` in the previously registered buffer.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto ReadFixed(IoUring &io_ring,
               fd_t fd,
               void *buf,
               size_t count,
               off_t offset,
               index_t buf_index,
               Func &&handler_fn = NoOpIoCompletionCb,
               const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_read_fixed(fd, offset, buf, count, buf_index, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Write request with a previously registered IO buffer that have been registered with io_uring_register_buffers.
/// @note The `buf` and `count` arguments must fall within a region specified by `buf_index` in the previously registered buffer.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto WriteFixed(IoUring &io_ring,
                fd_t fd,
                const void *buf,
                size_t count,
                off_t offset,
                index_t buf_index,
                Func &&handler_fn = NoOpIoCompletionCb,
                const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_write_fixed(fd, offset, buf, count, buf_index, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Poll the specified fd, equivalent to `poll(2)`
/// The default behavior is a single-shot poll request. When the specified event has triggered, a completion CQE is posted and no more
/// events will be generated by the poll request. If `multishot` is set to `true`behaviour is identical interms of event, but it persist
/// across notifications and will repeatedly post notifications for the same registration. A CQE posted from a multishot poll request will
/// have IORING_CQE_F_MORE set in the CQE flags member, indicating that the application should expect more completions from this request. If
/// the multishot poll request gets terminated or experiences an error, this flag will not be set in the CQE. If this happens, the
/// application should not expect further CQEs from the original request and must reissue a new one if it still wishes to get notifications
/// on this file descriptor.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto PollAdd(IoUring &io_ring,
             fd_t fd,
             flag_t events,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_poll_add(fd, events, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Remove an existing [poll](PollAdd) request.
/// The poll request identified by user_data set in sqe entry.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto PollRemove(IoUring &io_ring,
                void *user_data,
                Func &&handler_fn = NoOpIoCompletionCb,
                const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_poll_remove(user_data, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Remove an existing [poll](PollAdd) request.
/// The poll request identified by user_data set in sqe entry. If IORING_POLL_UPDATE_USER_DATA is enabled, user_data in sqe entry will be
/// replaced with new_user_data. The `events` arguments contains the new mask to use for the poll request, and `flags` argument contains
/// modifier flags telling io_uring what fields to update.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto PollUpdate(IoUring &io_ring,
                void *old_user_data,
                flag_t events,
                flag_t flags,
                Func &&handler_fn = NoOpIoCompletionCb,
                const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    IOURING_ASSERT(old_user_data, "old_user_data should be valid");
    std::unique_ptr<DefaultCompletion> comp = nullptr;
    if (flags & IORING_POLL_UPDATE_USER_DATA)
    {
        // If set, the poll update request will update the existing user_data of the
        // request with the value passed in as the new_user_data argument.
        comp = std::make_unique<DefaultCompletion>();
        comp->RegisterCallback(std::forward<Func>(handler_fn));

        // delete the old IoCompletion
        if (old_user_data)
        {
            delete ((IoCompletion *)(old_user_data));
        }
    }
    else
    {
        // retain old_user_data as sqe user_data
        // TODO: check if there is any notification in cqe flag to prevent destruction of user_data
        comp = std::make_unique<DefaultCompletion>(old_user_data);
    }
    if (flags & IORING_POLL_ADD_MULTI)
    {
        // IORING_POLL_ADD_MULTI
        //     If set, this will change the poll request from a singleshot to a multishot request.
        //     This must be used along with IORING_POLL_UPDATE_EVENTS as the event field must be updated
        //     to enable multishot.
        IOURING_ASSERT((flags & IORING_POLL_UPDATE_EVENTS), "must be used along with IORING_POLL_UPDATE_EVENTS");
    }
    auto req = IoRequest::make_poll_update(old_user_data, comp.get(), events, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Sync a file segment with disk, equivalent to `sync_file_range(2)`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto FileSyncRange(IoUring &io_ring,
                   fd_t fd,
                   size_t len,
                   off_t offset,
                   flag_t flags,
                   Func &&handler_fn = NoOpIoCompletionCb,
                   const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_sync_filerange(fd, len, offset, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Send a message on a socket, equivalent to `sendmsg(2)`.
/// `fd` must be set to the socket file descriptor, `iov` is vector of iovec structure, and `flags` holds the flags associated with the
/// system call.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto SendMsg(IoUring &io_ring,
             fd_t fd,
             const std::vector<iovec> &iov,
             flag_t flags,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<RecvSendMsgCompletion>(fd, iov);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_sendmsg(fd, comp->MsgHdr(), flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Send a message on a socket, equivalent to `send(2)`.
/// `fd` must be set to the socket file descriptor, addr must contains a pointer to the msghdr structure, and flags holds the flags
/// associated with the system call.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto SendMsg(IoUring &io_ring,
             fd_t fd,
             const ::msghdr &msghdr,
             flag_t flags,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<RecvSendMsgCompletion>(fd, msghdr);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_sendmsg(fd, comp->MsgHdr(), flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Receive a message on a socket, equivalent to `recvmsg(2)`.
/// `fd` must be set to the socket file descriptor, `iov` is vector of iovec structure, and `flags` holds the flags associated with the
/// system call.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto RecvMsg(IoUring &io_ring,
             fd_t fd,
             const std::vector<iovec> &iov,
             flag_t flags,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    if (common_vars.multishot_)
    {
        // The multishot version requires the IOSQE_BUFFER_SELECT flag to be set and no
        // MSG_WAITALL flag to be set
        IOURING_ASSERT((common_vars.buf_gid_ != IOURING_INVALID_BUFF_GID), "multishot version requires the IOSQE_BUFFER_SELECT to be set");
        IOURING_ASSERT((common_vars.sqe_flag_ & IOSQE_BUFFER_SELECT), "multishot version requires the IOSQE_BUFFER_SELECT to be set");
        IOURING_ASSERT((flags & MSG_WAITALL), "multishot version requires the MSG_WAITALL to be set");
    }
    auto comp = std::make_unique<RecvSendMsgCompletion>(fd, iov);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_recvmsg(fd, comp->MsgHdr(), flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Receive a message on a socket, equivalent to `recvmsg(2)`.
/// `fd` must be set to the socket file descriptor, addr must contains a pointer to the msghdr structure, and flags holds the flags
/// associated with the system call.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto RecvMsg(IoUring &io_ring,
             fd_t fd,
             const ::msghdr &msghdr,
             flag_t flags,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    if (common_vars.multishot_)
    {
        // The multishot version requires the IOSQE_BUFFER_SELECT flag to be set and no
        // MSG_WAITALL flag to be set
        IOURING_ASSERT((common_vars.buf_gid_ != IOURING_INVALID_BUFF_GID), "multishot version requires the IOSQE_BUFFER_SELECT to be set");
        IOURING_ASSERT((common_vars.sqe_flag_ & IOSQE_BUFFER_SELECT), "multishot version requires the IOSQE_BUFFER_SELECT to be set");
        IOURING_ASSERT((flags & MSG_WAITALL), "multishot version requires the MSG_WAITALL to be set");
    }
    auto comp = std::make_unique<RecvSendMsgCompletion>(fd, msghdr);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_recvmsg(fd, comp->MsgHdr(), flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Register a timeout operation.
/// A timeout will trigger a wakeup event on the completion ring for anyone waiting for events. A timeout condition is met when either the
/// specified timeout expires, or the specified number of events have completed. Either condition will trigger the event. The request will
/// complete with `-ETIME` if the timeout got completed through expiration of the timer, or 0 if the timeout got completed through requests
/// completing on their own. If the timeout was cancelled before it expired, the request will complete with `-ECANCELED`.
/// @note `timeout` is specified in nanoseconds
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Timeout(IoUring &io_ring,
             std::chrono::nanoseconds &timeout,
             uint32_t count,
             flag_t flags = 0,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_timeout(timeout, count, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Attempt to remove/cancel an existing timeout operation.
/// The submission queue entry sqe is setup to arm a timeout removal specified by user_data and with modifier flags given by flags.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto TimeoutRemove(IoUring &io_ring,
                   pointer_t user_data,
                   flag_t flags = 0,
                   Func &&handler_fn = NoOpIoCompletionCb,
                   const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_timeout_remove(user_data, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Attempt to update an existing timeout operation.
/// The submission queue entry sqe is setup to arm a timeout removal specified by user_data and with modifier flags given by flags and  ts
/// structure, which contains new timeout information
/// @note `timeout` is specified in nanoseconds
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto TimeoutUpdate(IoUring &io_ring,
                   std::chrono::nanoseconds &timeout,
                   pointer_t user_data,
                   flag_t flags = 0,
                   Func &&handler_fn = NoOpIoCompletionCb,
                   const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_timeout_update(timeout, user_data, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Accept a new connection on a socket, equivalent to `accept4(2)`.
///  The file descriptor `sockfd` to start accepting a connection request described by the socket address at `addr` and of structure length
///  `addrlen` and using modifier flags in `flags`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Accept(IoUring &io_ring,
            fd_t sockfd,
            socket_addr_t *addr,
            socket_len_t *addrlen,
            flag_t flags,
            Func &&handler_fn = NoOpIoCompletionCb,
            const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(sockfd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_accept(sockfd, addr, addrlen, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Attempt to cancel an already issued request.
/// The submission queue entry sqe is prepared to cancel an existing request identified by user_data.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Cancel(IoUring &io_ring,
            void *user_data,
            flag_t flags,
            Func &&handler_fn = NoOpIoCompletionCb,
            const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_cancel(user_data, 0, IOURING_INVALID_SOCKET, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Attempt to cancel an already issued request.
/// The submission queue entry sqe is prepared to cancel an existing request identified by user_data 64-bit integer rather than a pointer
/// type.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Cancel64(IoUring &io_ring,
              uint64_t user_data,
              flag_t flags,
              Func &&handler_fn = NoOpIoCompletionCb,
              const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_cancel(nullptr, user_data, IOURING_INVALID_SOCKET, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Attempt to cancel an already issued request.
/// The submission queue entry sqe is prepared to cancel an existing request that used the file descriptor fd.
/// The cancelation request will attempt to find the previously issued request that used fd as the file descriptor and cancel it.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto CancelFd(IoUring &io_ring,
              fd_t fd,
              flag_t flags,
              Func &&handler_fn = NoOpIoCompletionCb,
              const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_cancel(nullptr, 0, fd, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief This request must be linked with another request through `Flags::IO_LINK`
/// Unlike `Timeout`, `LinkTimeout` acts on the linked request, not the completion queue.
/// A timeout condition is met when either the specified timeout expires, or the linked sqes are completed. The request will
/// complete with `-ETIME` if the timeout got completed through expiration of the timer, or 0 if the timeout got completed through linked
/// requests completing on their own. If the timeout was cancelled before it expired, the request will complete with `-ECANCELED`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto LinkTimeout(IoUring &io_ring,
                 std::chrono::nanoseconds &timeout,
                 flag_t flags = 0,
                 Func &&handler_fn = NoOpIoCompletionCb,
                 const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_link_timeout(timeout, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Connect a socket, equivalent to `connect(2)`.
/// The file descriptor `sockfd` to start connecting to the destination described by the socket address at `addr` and of structure length
/// `addrlen`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Connect(IoUring &io_ring,
             fd_t sockfd,
             const socket_addr_t *addr,
             socket_len_t addrlen,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(sockfd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_connect(sockfd, addr, addrlen, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Preallocate or deallocate space to a file, equivalent to `fallocate(2)`.
/// The file descriptor pointed to by `fd` to start a fallocate operation described by mode at offset `offset` and `len` length in bytes.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Fallocate(IoUring &io_ring,
               fd_t fd,
               flag_t mode,
               offset_t offset,
               io_size_t len,
               Func &&handler_fn = NoOpIoCompletionCb,
               const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_fallocate(fd, mode, offset, len, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Open a file, equivalent to `openat(2)`.
/// The directory file descriptor `dirfd` to start opening a file described by `pathname` and using the open flags in `flags` and using the
/// file mode bits specified in `mode`.
/// For a direct descriptor open request, the offset is specified by the `file_index` argument in `IoApiCommonVars`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Openat(IoUring &io_ring,
            fd_t dirfd,
            const char *pathname,
            flag_t flags,
            flag_t mode,
            Func &&handler_fn = NoOpIoCompletionCb,
            const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_openat(dirfd, pathname, flags, mode, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Close a file descriptor, equivalent to `close(2)`.
/// Close the file descriptor indicated by fd.
/// For a direct descriptor open request, the offset is specified by the `file_index` argument in `IoApiCommonVars`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Close(IoUring &io_ring, fd_t fd, Func &&handler_fn = NoOpIoCompletionCb, const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_close(fd, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief For updating a number of previously registered file descriptors.
/// Close the file descriptor indicated by fd.
/// The file descriptor array pointed to by vector `fds` of specific length to update that amount of previously registered files starting at
/// offset `offset`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto FilesUpdate(IoUring &io_ring,
                 std::vector<fd_t> &fds,
                 int offset,
                 Func &&handler_fn = NoOpIoCompletionCb,
                 const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_files_update(fds, offset, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Get file status, equivalent to `statx(2)`.
///  The directory file descriptor pointed to by `dirfd` to start a statx operation on the path identified by `pathname` and using the flags
///  given in `flags` for the fields specified by `mask` and into the buffer located at `statxbuf`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Statx(IoUring &io_ring,
           fd_t dirfd,
           const char *pathname,
           flag_t flags,
           flag_t mask,
           struct statx *statxbuf,
           Func &&handler_fn = NoOpIoCompletionCb,
           const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<StatxCompletion>(pathname, statxbuf);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_statx(dirfd, pathname, flags, mask, statxbuf, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief The equivalent of a `read(2)`/pread(2).
/// The file descriptor `fd` to start reading `count` bytes into the buffer `buf` at the specified `offset`.
/// To read the data to provided buffers register using `io_uring_prep_provide_buffers` `[IoUringParams::AddProvidedBuffer]`
/// set `buf` as nullptr and set `buf_gid` in `common_vars`. `IOSQE_BUFFER_SELECT` will set in such case.
/// @note On files that are not capable of seeking, the offset must be 0 or -1.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Read(IoUring &io_ring,
          fd_t fd,
          void *buf,
          size_t count,
          off_t offset,
          Func &&handler_fn = NoOpIoCompletionCb,
          const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    IOURING_ASSERT((buf || (common_vars.buf_gid_ != IOURING_INVALID_BUFF_GID)), "Either buf should be valid or buf_gid should be valid");
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_read(fd, offset, buf, count, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief The equivalent of a `write(2)`/`pwrite(2)`.
/// The file descriptor `fd` to start writing `count` from the buffer `buf` at the specified offset.
/// @note On files that are not capable of seeking, the offset must be 0 or -1.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Write(IoUring &io_ring,
           fd_t fd,
           const void *buf,
           size_t count,
           off_t offset,
           Func &&handler_fn = NoOpIoCompletionCb,
           const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_write(fd, offset, buf, count, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Predeclare an access pattern for file data, equivalent to `posix_fadvise(2)`.
/// The file descriptor pointed to by `fd` to start an fadvise operation at `offset` and of `len` length in bytes, giving it the advise
/// located in `advice`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Fadvise(IoUring &io_ring,
             fd_t fd,
             off_t offset,
             io_size_t len,
             int advice,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(fd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_fadvise(fd, offset, len, advice, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Give advice about use of memory, equivalent to `madvise(2)`.
/// Start an madvise operation at the virtual address of `addr` and of `len` length in bytes, giving it the advise located in `advice`.
/// located in `advice`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Madvise(IoUring &io_ring,
             void *addr,
             io_size_t len,
             int advice,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_madvise(addr, len, advice, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Send a message on a socket, equivalent to `send(2)`.
/// The file descriptor `sockfd` to start sending the data from `buf` of size `len` bytes and with modifier flags loaded in `flags`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Send(IoUring &io_ring,
          fd_t sockfd,
          const void *buf,
          size_t len,
          flag_t flags,
          Func &&handler_fn = NoOpIoCompletionCb,
          const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(sockfd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_send(sockfd, buf, len, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Receive a message from a socket, equivalent to `recv(2)`.
/// The file descriptor `sockfd` to start receiving the data into the buffer `buf` of size `len` bytes and with modifier flags loaded in
/// `flags`. For multishot version, set `multishot` argument in `IoApiCommonVars`. The multishot version requires length to be 0.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Recv(IoUring &io_ring,
          fd_t fd,
          void *buf,
          size_t len,
          flag_t flags,
          Func &&handler_fn = NoOpIoCompletionCb,
          const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    IOURING_ASSERT((buf || (common_vars.buf_gid_ != IOURING_INVALID_BUFF_GID)), "Either buf should be valid or buf_gid should be valid");
    // The multishot version requires length to be 0 , the IOSQE_BUFFER_SELECT flag to be set and no MSG_WAITALL flag to be set.
    // For IOSQE_BUFFER_SELECT, buf should be nullptr.
    IOURING_ASSERT((!common_vars.multishot_ || buf), "Multishot condition is not satisfied");
    IOURING_ASSERT((!common_vars.multishot_ || ((len == 0) && (common_vars.buf_gid_ != IOURING_INVALID_BUFF_GID))),
                   "Multishot condition is not satisfied");
    auto comp = std::make_unique<RwProvidedBufferCompletion>(fd, common_vars.buf_gid_);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_recv(fd, buf, len, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Open a file, equivalent to `openat2(2)`.
/// The directory file descriptor `dirfd` to start opening a file described by `pathname` and using the open flags in `flags` and using the
/// instructions on how to open the file given in `how`. For a direct descriptor open request, the offset is specified by the `file_index`
/// argument in `IoApiCommonVars`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Openat2(IoUring &io_ring,
             fd_t dirfd,
             const char *pathname,
             struct open_how *how,
             flag_t flags,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_openat2(dirfd, pathname, flags, how, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Modify an epoll file descriptor, equivalent to `epoll_ctl(2)`.
/// The epoll(7) instance referred to by the file descriptor `epfd`.  It requests that the operation `op` be performed for the target file
/// descriptor, `fd`. The `event` argument describes the object linked to the file descriptor `fd`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto EpollCtl(IoUring &io_ring,
              fd_t epfd,
              fd_t fd,
              int op,
              struct epoll_event *event,
              Func &&handler_fn = NoOpIoCompletionCb,
              const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_epoll_ctl(epfd, fd, op, event, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Splice data to/from a pipe, equivalent to `splice(2)`.
///  Then input the file descriptor `in_fd` at offset `in_offset`, splicing data to the file descriptor at `out_fd` and at offset
///  `out_offset`. `size` bytes of data should be spliced between the two descriptors. `flags` are modifier flags for the operation.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Splice(IoUring &io_ring,
            fd_t in_fd,
            offset_t in_offset,
            fd_t out_fd,
            offset_t out_offset,
            io_size_t size,
            flag_t flags,
            Func &&handler_fn = NoOpIoCompletionCb,
            const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    // TODO: Add support for below requirement
    //  If the fd_out descriptor, IOSQE_FIXED_FILE can be set in the SQE to indicate that. For the input file, the io_uring specific
    //  SPLICE_F_FD_IN_FIXED can be set in splice_flags and fd_in given as a registered file descriptor offset.
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_splice(in_fd, in_offset, out_fd, out_offset, size, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Register `nbufs` number of buffers starting at `buffer` each of length `len` and identified by the buffer group ID of `gid` and
/// numbered sequentially starting at `bid` to be used in any read or receive operations with IOSQE_BUFFER_SELECT flag.
/// @note if `len` is given as `0`, then buffer length will be determined based on `gid`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto ProvideBuffer(IoUring &io_ring,
                   char_t *buffer,
                   io_size_t len,
                   uint32_t nbufs,
                   const io_buf_gid_t &gid,
                   const io_buf_bid_t &bid,
                   Func &&handler_fn = NoOpIoCompletionCb,
                   const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_provide_buffer(buffer, len, nbufs, gid, bid, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Removes previously registered buffer using `ProvideBuffer [io_uring_prep_provide_buffers]`.
/// Removes `nbufs` number of buffers from the buffer group ID indicated by `gid`.
/// @note If `nbufs` is given as `0`, then all the buffers associated with `gid` will be removed.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto RemoveBuffer(IoUring &io_ring,
                  uint32_t nbufs,
                  const io_buf_gid_t &gid,
                  Func &&handler_fn = NoOpIoCompletionCb,
                  const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_remove_buffer(nbufs, gid, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Duplicate pipe content, equivalent to `tee(2)`.
/// The input file descriptor `in_fd` and as output the file descriptor `out_fd` duplicating `size` bytes worth of data. `flags` are
/// modifier flags for the operation.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Tee(IoUring &io_ring,
         fd_t in_fd,
         fd_t out_fd,
         io_size_t size,
         flag_t flags,
         Func &&handler_fn = NoOpIoCompletionCb,
         const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    // TODO: Add support for below requirement
    //  If the fd_out descriptor, IOSQE_FIXED_FILE can be set in the SQE to indicate that. For the input file, the io_uring specific
    //  SPLICE_F_FD_IN_FIXED can be set in splice_flags and fd_in given as a registered file descriptor offset.
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_tee(in_fd, out_fd, size, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Shutdown connection on the socket associated with sockfd, equivalent to `shutdown(2)`.
/// The file descriptor `sockfd` that should be shutdown with the `how` argument.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Shutdown(IoUring &io_ring,
              fd_t sockfd,
              flag_t how,
              Func &&handler_fn = NoOpIoCompletionCb,
              const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_shutdown(sockfd, how, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Peform reame operation, equivalent to ` renameat2(2) or rename(2)`.
/// The old directory file descriptor pointed to by `olddirfd` and old path pointed to by `oldpath` with the new directory file descriptor
/// pointed to by `newdirfd` and the new path pointed to by `newpath` and using the specified flags in `flags`.
/// If `olddirfd` and/or `newdirfd` is -1, then `rename(2)` equivalent operation is triggered.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto Renameat(IoUring &io_ring,
              fd_t olddirfd,
              const char *oldpath,
              fd_t newdirfd,
              const char *newpath,
              flag_t flags,
              Func &&handler_fn = NoOpIoCompletionCb,
              const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_renameat(olddirfd, oldpath, newdirfd, newpath, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Removes name from filesystem, equivalent to `unlinkat(2)` or `unlink(2)`.
/// The directory file descriptor pointed to by `dirfd` to start an unlinkat operation on the path identified by `path` and using the flags
/// given in `flags`. If `dirfd` is set to -1, then `unlink(2)` version is triggered.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Uninkat(IoUring &io_ring,
             fd_t dirfd,
             const char *path,
             flag_t flags,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_unlinkat(dirfd, path, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Make a directory, equivalent to `mkdirat2(2)` or `mkdir(2)`.
/// The directory file descriptor pointed to by `dirfd` to start a mkdirat operation on the path identified by `path` with the mode given in
/// `mode`. If `dirfd` is set to -1, then `mkdir(2)` version is triggered.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Makrdirat(IoUring &io_ring,
               fd_t dirfd,
               const char *path,
               flag_t mode,
               Func &&handler_fn = NoOpIoCompletionCb,
               const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_makedirat(dirfd, path, mode, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Create a symlink, equivalent to `symlinkat2(2)` or `symlink(2)`.
/// symlink the target path pointed to by `target` to the new destination indicated by `newdirfd` and `linkpath`.
/// If `newdirfd` is set to -1, then `symlink(2)` version is triggered.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Symlinkat(IoUring &io_ring,
               const char *target,
               fd_t newdirfd,
               const char *linkpath,
               Func &&handler_fn = NoOpIoCompletionCb,
               const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_symlinkat(target, newdirfd, linkpath, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Create a hard link, equivalent to `linkat2(2)` or `link(2)`.
/// The old directory file descriptor pointed to by `olddirfd` and old path pointed to by `oldpath` with the new directory file descriptor
/// pointed to by `newdirfd` and the new path pointed to by `newpath` and using the specified flags in `flags`.
/// If `olddirfd` and/or `newdirfd` is -1, then `link(2)` equivalent operation is triggered.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Linkat(IoUring &io_ring,
            fd_t olddirfd,
            const char *oldpath,
            fd_t newdirfd,
            const char *newpath,
            flag_t flags,
            Func &&handler_fn = NoOpIoCompletionCb,
            const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_linkat(olddirfd, oldpath, newdirfd, newpath, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Sending a “message” from one ring to another, one 64-bit value and one 32-bit value
/// The file descriptor `fd`, which must identify a io_uring context
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto MsgRing(IoUring &io_ring,
             fd_t fd,
             const uint32_t &data_32,
             const uint64_t &data_64,
             flag_t flags,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_msgring(fd, data_32, data_64, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Set an extended attribute value, equivalent to `fsetxattr(2)`
/// Sets the `value` of the extended attribute identified by `name` and associated file descriptor `fd` and using the specified flags in
/// `flags`. The `len` argument specifies the size (in bytes) of `value`; a zero-length value is permitted.
/// @note `len` should be set to `0`.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Fsetxattr(IoUring &io_ring,
               fd_t fd,
               const char *name,
               const char *value,
               flag_t flags,
               io_size_t len,
               Func &&handler_fn = NoOpIoCompletionCb,
               const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_fsetxattr(fd, name, value, flags, len, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Set an extended attribute value, equivalent to `setxattr(2)`
/// Sets the `value` of the extended attribute identified by `name` and associated with the given `path` and using the specified flags in
/// `flags`. The `len` argument specifies the size (in bytes) of `value`; a zero-length value is permitted.
/// @note `len` should be set to `0`.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Setxattr(IoUring &io_ring,
              const char *path,
              const char *name,
              const char *value,
              flag_t flags,
              io_size_t len,
              Func &&handler_fn = NoOpIoCompletionCb,
              const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_setxattr(path, name, value, flags, len, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Get an extended attribute value, equivalent to `fgetxattr(2)`
/// Retrieves  the value of the extended attribute identified by `name` and associated file descriptor `fd`.
/// The attribute value is placed in the buffer pointed to by `value`; `len` specifies the size of that buffer.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Fgetxattr(IoUring &io_ring,
               fd_t fd,
               const char *name,
               const char *value,
               io_size_t len,
               Func &&handler_fn = NoOpIoCompletionCb,
               const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_fgetxattr(fd, name, value, len, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Get an extended attribute value, equivalent to `getxattr(2)`
/// Retrieves  the value of the extended attribute identified by `name` and and associated with the given `path`.
/// The attribute value is placed in the buffer pointed to by `value`; `len` specifies the size of that buffer.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Getxattr(IoUring &io_ring,
              const char *path,
              const char *name,
              const char *value,
              io_size_t len,
              Func &&handler_fn = NoOpIoCompletionCb,
              const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_getxattr(path, name, value, len, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Creates a socket, equivalent to `socket(2)`
/// THe `domain` specifies the communication domain, type specified in `type` and protocol specified in `protocol`.
/// For a direct descriptor open request, the offset is specified by the `file_index` argument in `IoApiCommonVars`.
/// @return  IoFuture instance
template<typename Func = IoCompletionCb>
auto Socket(IoUring &io_ring,
            int domain,
            int type,
            int protocol,
            flag_t flags,
            Func &&handler_fn = NoOpIoCompletionCb,
            const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<SocketCompletion>();
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_socket(domain, type, protocol, flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Send a message on a socket, zerocopy equivalent to `send(2)`.
/// The file descriptor `sockfd` to start sending the data from `buf` of size `len` bytes and with modifier flags loaded in `flags` and zero copy flags in `zc_flags`.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto SendZc(IoUring &io_ring,
          fd_t sockfd,
          const void *buf,
          size_t len,
          flag_t flags,
          flag_t zc_flags,
          Func &&handler_fn = NoOpIoCompletionCb,
          const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<DefaultCompletion>(sockfd);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_sendzc(sockfd, buf, len, flags, zc_flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

/// @brief Send a message on a socket, zerocopy equivalent to `sendmsg(2)`.
/// `fd` must be set to the socket file descriptor, addr must contains a pointer to the msghdr structure, and flags holds the flags
/// associated with the system call.
/// @return IoFuture instance
template<typename Func = IoCompletionCb>
auto SendMsgZc(IoUring &io_ring,
             fd_t fd,
             const ::msghdr &msghdr,
             flag_t flags,
             Func &&handler_fn = NoOpIoCompletionCb,
             const IoApiCommonVars &common_vars = IoApiCommonVars())
{
    auto comp = std::make_unique<RecvSendMsgCompletion>(fd, msghdr);
    comp->RegisterCallback(std::forward<Func>(handler_fn));
    auto req = IoRequest::make_sendmsgzc(fd, comp->MsgHdr(), flags, common_vars);
    return io_ring.SubmitIoRequest(std::move(req), comp.release());
}

} // namespace iouring
