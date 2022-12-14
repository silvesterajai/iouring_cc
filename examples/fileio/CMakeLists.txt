set(target fastio_fileio)

set(source_dir ${CMAKE_CURRENT_SOURCE_DIR})
set(INCLUDES ${source_dir})
set(SOURCES ${source_dir}/fileio.cc)
set(DEPENDENCIES liburing spdlog::spdlog fastio)
set(_DEFINES ENABLE_LOGGING UNIT_TEST HAVE_LINUX_TIME_TYPES_H)
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
