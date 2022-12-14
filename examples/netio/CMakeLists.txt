set(target fastio_netio)

set(source_dir ${CMAKE_CURRENT_SOURCE_DIR})
set(INCLUDES ${source_dir})
set(SOURCES 
    ${source_dir}/socket_ops.cc
    ${source_dir}/epoll_ops.cc
    ${source_dir}/socket.cc
    ${source_dir}/socket_ops.cc
    ${source_dir}/inetaddress.cc
    ${source_dir}/acceptor.cc
    ${source_dir}/epoll.cc)
set(DEPENDENCIES liburing spdlog::spdlog fastio)
set(_DEFINES ENABLE_LOGGING UNIT_TEST USE_IOURING HAVE_LINUX_TIME_TYPES_H)
cc_add_library(
    NAME
    ${target}
    SHARED
    SRCS
    "${SOURCES}"
    INCLUDES
    "${INCLUDES}"
    DEPS
    "${DEPENDENCIES}"
    DEFINES
    "${_DEFINES}")
if(CODE_COVERAGE)
    target_code_coverage(${target} SHARED)
endif()

set(target tcp_server)
cc_add_executable(
    NAME
    ${target}
    SHARED
    SRCS
    ${source_dir}/tcp_server.cc
    INCLUDES
    "${INCLUDES}"
    DEPS
    fastio_netio
    DEFINES
    "${_DEFINES}")

set(target tcp_client)
cc_add_executable(
    NAME
    ${target}
    SHARED
    SRCS
    ${source_dir}/tcp_client.cc
    INCLUDES
    "${INCLUDES}"
    DEPS
    fastio_netio
    DEFINES
    "${_DEFINES}")

set(target epoll_tcp_server)
cc_add_executable(
    NAME
    ${target}
    SHARED
    SRCS
    ${source_dir}/epoll_tcp_server.cc
    INCLUDES
    "${INCLUDES}"
    DEPS
    fastio_netio
    DEFINES
    "${_DEFINES}")
