set(target iouring_gtest)

set(source_dir ${CMAKE_CURRENT_SOURCE_DIR})
# include the current directory
set(INCLUDES ${source_dir})
# cmake-format off
set(SOURCES ${source_dir}/gtest_main.cc 
            ${source_dir}/test_io_uring.cc
            ${source_dir}/test_fileio.cc 
            ${source_dir}/test_file_register.cc)
# cmake-format on
set(DEPENDENCIES GTest::gtest_main spdlog::spdlog fastio)
set(_DEFINES ENABLE_LOGGING UNIT_TEST IOURING_STATS HAVE_LINUX_TIME_TYPES_H)
cc_add_test(
    NAME
    ${target}
    SRCS
    "${SOURCES}"
    DEPS
    "${DEPENDENCIES}"
    INCLUDES
    "${INCLUDES}"
    DEFINES
    "${_DEFINES}")

if(CODE_COVERAGE)
    target_code_coverage(${target} AUTO)
endif()
