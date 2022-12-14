if(NOT USE_TIMEMORY)
    return()
endif()
set(target timemory-interface)

set(source_dir ${CMAKE_CURRENT_SOURCE_DIR})
# include the current directory
set(INCLUDES ${source_dir})
set(SOURCES ${source_dir}/timemory_interface.cc)
cc_add_library(
    NAME
    ${target}
    SHARED
    SRCS
    "${SOURCES}"
    INCLUDES
    "${INCLUDES}"
    DEPS
    timemory-extern-interface)
