set(target fastio)

set(source_dir ${CMAKE_CURRENT_SOURCE_DIR})
set(INCLUDES ${source_dir})
set(SOURCES ${source_dir}/io_uring.cc)
set(DEPENDENCIES liburing spdlog::spdlog)
set(_DEFINES HAVE_LINUX_TIME_TYPES_H)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_DEFINES "${_DEFINES}" ENABLE_LOGGING UNIT_TEST IOURING_STATS)
endif()
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
