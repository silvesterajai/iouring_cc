#pragma once

#include "iouring/status.h"
#include "iouring/io_uring_utils.h"
#include <arpa/inet.h>

namespace iouring {
namespace net {
namespace ops {
int CreateSocket(sa_family_t family, int type, int protocol);
void Bind(int sockfd, const struct sockaddr *addr);
int Connect(int sockfd, const struct sockaddr *addr);
void Listen(int sockfd);
int Accept(int sockfd, struct sockaddr *addr);
ssize_t Read(int sockfd, void *buf, size_t count);
ssize_t Readv(int sockfd, const struct iovec *iov, int iovcnt);
ssize_t Write(int sockfd, const void *buf, size_t count);
ssize_t Close(int sockfd);
ssize_t Send(int sockfd, const void *buf, size_t count, int flags=0);
ssize_t Recv(int sockfd, void *buf, size_t count, int flags=0);
const BufferInfo Recv(int sockfd, const size_t &count, int flags=0);
void ProvideBuffer(char_t *buffer, const io_buf_gid_t &gid, const io_buf_bid_t &bid);

const struct sockaddr *sockaddr_cast(const struct sockaddr_in6 *addr);
struct sockaddr *sockaddr_cast(struct sockaddr_in6 *addr);
const struct sockaddr *sockaddr_cast(const struct sockaddr_in *addr);
struct sockaddr *sockaddr_cast(struct sockaddr_in *addr);
const struct sockaddr_in *sockaddr_in_cast(const struct sockaddr *addr);
const struct sockaddr_in6 *sockaddr_in6_cast(const struct sockaddr *addr);

} // namespace ops
} // namespace net
} // namespace iouring