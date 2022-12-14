#pragma once
#include <arpa/inet.h>
#include <stdint.h>

namespace iouring {
namespace net {
inline uint32_t HostToNetwork32(uint32_t host32)
{
    return htonl(host32);
}

inline uint16_t HostToNetwork16(uint16_t host16)
{
    return htons(host16);
}

inline uint32_t NetworkToHost32(uint32_t net32)
{
    return ntohl(net32);
}

inline uint16_t NetworkToHost16(uint16_t net16)
{
    return ntohs(net16);
}

} // namespace net
} // namespace iouring