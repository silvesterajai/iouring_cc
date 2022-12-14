include(CMakeParseArguments)

# generic library for compiler flags, include dirs, link libs, etc.
add_library(cc-compile-flags INTERFACE)
# enable warnings
target_compile_options(cc-compile-flags INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17 -W
                                                  -Wall -Werror>)
target_link_libraries(cc-compile-flags INTERFACE -Wl,--no-as-needed -ldl)
# add include directories
target_include_directories(cc-compile-flags BEFORE INTERFACE ${CMAKE_SOURCE_DIR})

# cmake-format: off
##------------------------------------------------------------------------------
## cc_add_sanitizer_flag( NAME        <name>)
##
## Adds sanitizer flags to target, called <name>.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_add_sanitizer_flag)
    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME" # single value argumentes
        "" # multi value argumentes
        ${ARGN})
    # If no sanitizer is enabled, return immediately.
    if(NOT
       (SANITIZE_ADDRESS
        OR SANITIZE_MEMORY
        OR SANITIZE_THREAD
        OR SANITIZE_UNDEFINED))
        return()
    endif()

    if(SANITIZE_ADDRESS AND (SANITIZE_THREAD OR SANITIZE_MEMORY))
        message(FATAL_ERROR "AddressSanitizer is not compatible with "
                            "ThreadSanitizer or MemorySanitizer.")
    endif()
    get_target_property(TARGET_TYPE ${_RULE_NAME} TYPE)
    if(TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
        message(WARNING "Can't use any sanitizers for target ${_RULE_NAME}, "
                        "because it is an interface library and cannot be "
                        "compiled directly.")
        return()
    endif()

    if(SANITIZE_ADDRESS)
        find_package(ASan REQUIRED)
        set(SANITIZE_ADDRESS_FLAGS -g -fsanitize=address -fno-omit-frame-pointer)
        cc_target_compile_options(NAME ${_RULE_NAME} COPTS ${SANITIZE_ADDRESS_FLAGS})
        cc_target_link_libraries(NAME ${_RULE_NAME} DEPS ${ASan_LIBRARY})
        # cc_target_link_libraries(NAME ${_RULE_NAME} ${SANITIZE_ADDRESS_FLAGS})
    endif()

    if(SANITIZE_MEMORY)
        find_package(MSan REQUIRED)
        set(SANITIZE_MEMORY_FLAGS -g -fsanitize=memory)
        cc_target_compile_options(NAME ${_RULE_NAME} COPTS ${SANITIZE_MEMORY_FLAGS})
        cc_target_link_libraries(NAME ${_RULE_NAME} DEPS ${MSan_LIBRARY})
        # cc_target_link_libraries(NAME ${_RULE_NAME} ${SANITIZE_MEMORY_FLAGS})
    endif()

    if(SANITIZE_THREAD)
        find_package(TSan REQUIRED)
        set(SANITIZE_THREAD_FLAGS -g -fsanitize=thread)
        cc_target_compile_options(NAME ${_RULE_NAME} COPTS ${SANITIZE_THREAD_FLAGS})
        cc_target_link_libraries(NAME ${_RULE_NAME} DEPS ${TSan_LIBRARY})
        # cc_target_link_libraries(NAME ${_RULE_NAME} ${SANITIZE_THREAD_FLAGS})
    endif()

    if(SANITIZE_UNDEFINED)
        find_package(UBSan REQUIRED)
        set(SANITIZE_UNDEFINED_FLAGS -g -fsanitize=undefined)
        cc_target_compile_options(NAME ${_RULE_NAME} COPTS ${SANITIZE_UNDEFINED_FLAGS})
        cc_target_link_libraries(NAME ${_RULE_NAME} DEPS ${UBSan_LIBRARY})
        # cc_target_link_libraries(NAME ${_RULE_NAME} ${SANITIZE_UNDEFINED_FLAGS})
    endif()

endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_enable_profiling( NAME        <name>)
##
## Enable profiling to target, called <name>.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_enable_profiling)
    if(NOT USE_TIMEMORY AND NOT USE_PAPI)
        return()
    endif()

    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME" # single value argumentes
        "" # multi value argumentes
        ${ARGN})
    if(${_RULE_NAME} STREQUAL "timemory-interface")
        return()
    endif()
    if(USE_TIMEMORY)
        # set(timemory_FIND_COMPONENTS_INTERFACE timemory-extern-interface) find_package(
        # timemory REQUIRED COMPONENTS headers compile-options OPTIONAL_COMPONENTS cxx
        # papi)
        cc_target_link_libraries(NAME ${_RULE_NAME} DEPS timemory-interface)
        cc_target_compile_definitions(NAME ${_RULE_NAME} COPTS USE_TIMEMORY)
        list(APPEND CMAKE_INSTALL_RPATH ${timemory_ROOT_DIR}/lib
             ${timemory_ROOT_DIR}/lib64)
    endif()

    if(USE_PAPI)
        if(USE_PAPI)
            if(NOT timemory_FOUND)
                find_package(timemory QUIET)
            endif()
            if(timemory_DIR)
                list(APPEND CMAKE_MODULE_PATH ${timemory_DIR}/Modules)
            endif()
            find_package(PAPI REQUIRED)
            cc_target_link_libraries(NAME ${_RULE_NAME} DEPS PAPI::papi-static)
            cc_target_compile_definitions(NAME ${_RULE_NAME} COPTS PAPI)
        endif()
    endif()
endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_target_compile_options( NAME        <name>
##                            COPTS     [opt1 [opt2 ...]])
##
## Adds compile options to target, called <name>.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_target_compile_options)
    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME" # single value argumentes
        "COPTS" # multi value argumentes
        ${ARGN})
    if(_RULE_COPTS)
        logger(UNIT "${_RULE_NAME}" LEVEL "DEBUG" "target_compile_options ${_RULE_COPTS}")
        target_compile_options(${_RULE_NAME} PRIVATE ${_RULE_COPTS})
    endif()
endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_target_link_options( NAME        <name>
##                         LOPTS     [opt1 [opt2 ...]])
##
## Adds link options to target, called <name>.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_target_link_options)
    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME" # single value argumentes
        "LINKOPTS" # multi value argumentes
        ${ARGN})
    if(_RULE_LINKOPTS)
        logger(UNIT "${_RULE_NAME}" LEVEL "DEBUG" "target_link_options ${_RULE_LINKOPTS}")
        target_link_options(${_RULE_NAME} PRIVATE ${_RULE_LINKOPTS})
    endif()
endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_target_link_options( NAME        <name>
##                         LOPTS     [opt1 [opt2 ...]])
##
## Adds link options to target, called <name>.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_target_link_libraries)
    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME" # single value argumentes
        "DEPS" # multi value argumentes
        ${ARGN})
    if(_RULE_DEPS)
        logger(UNIT "${_RULE_NAME}" LEVEL "DEBUG" "target_link_libraries ${_RULE_DEPS}")
        get_target_property(IS_INTERFACE ${_RULE_NAME} TYPE)
        if(NOT IS_INTERFACE STREQUAL "INTERFACE_LIBRARY")
            set(_TYPE "PUBLIC")
        else()
            set(_TYPE "INTERFACE")
        endif()
        target_link_libraries(${_RULE_NAME} ${_TYPE} ${_RULE_DEPS})
    endif()
endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_target_include_directories( NAME        <name>
##                            INCLUDES     [inc1 [inc2 ...]])
##
## Adds link options to target, called <name>.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_target_include_directories)
    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME" # single value argumentes
        "INCLUDES" # multi value argumentes
        ${ARGN})
    if(_RULE_INCLUDES)
        logger(UNIT "${_RULE_NAME}" LEVEL "DEBUG"
                                          "target_include_directories ${_RULE_INCLUDES}")
        get_target_property(IS_INTERFACE ${_RULE_NAME} TYPE)
        if(NOT IS_INTERFACE STREQUAL "INTERFACE_LIBRARY")
            set(_TYPE "PUBLIC")
        else()
            set(_TYPE "INTERFACE")
        endif()
        target_include_directories(${_RULE_NAME} ${_TYPE} ${_RULE_INCLUDES})
    endif()
endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_target_compile_definitions( NAME        <name>
##                                DEFINES     [define1 [define2 ...]])
##
## Adds link options to target, called <name>.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_target_compile_definitions)
    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME" # single value argumentes
        "DEFINES" # multi value argumentes
        ${ARGN})
    if(_RULE_DEFINES)
        logger(UNIT "${_RULE_NAME}" LEVEL "DEBUG"
                                          "target_compile_definitions ${_RULE_DEFINES}")
        target_compile_definitions(${_RULE_NAME} PRIVATE ${_RULE_DEFINES})
    endif()
endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_add_executable( NAME        <name>
##                     SOURCES     [source1 [source2 ...]]
##                     HEADERS     [header1 [header2 ...]]
##                     INCLUDES    [dir1 [dir2 ...]]
##                     DEFINES     [define1 [define2 ...]]
##                     DEPENDS_ON  [dep1 [dep2 ...]]
##                     OUTPUT_DIR  [dir]
##                     OUTPUT_NAME [name]
##                     FOLDER      [name])
##
## Adds an executable target, called <name>, to be built from the given sources.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_add_executable)
    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME;OUTPUT_NAME" # single value argumentes
        "SRCS;HDRS;INCLUDES;DEFINES;COPTS;LINKOPTS;DEPS;TAGS" # multi value argumentes
        ${ARGN})
    # Sanity checks
    if("${_RULE_NAME}" STREQUAL "")
        message(
            FATAL_ERROR "cc_add_executable() must be called with argument NAME <name>")
    endif()
    if(NOT _RULE_SRCS)
        message(
            FATAL_ERROR "cc_add_executable(NAME ${_RULE_NAME} ...) given with no sources")
    endif()
    logger(UNIT ${_RULE_NAME} LEVEL "STATUS"
                                    "Adding executable ${_RULE_NAME} ${_RULE_SRCS}")
    add_executable(${_RULE_NAME} ${_RULE_SRCS} ${_RULE_HDRS})
    if(_RULE_OUTPUT_NAME)
        set_target_properties(${_RULE_NAME} PROPERTIES OUTPUT_NAME ${_RULE_OUTPUT_NAME})
    endif()
    if(_RULE_OUTPUT_DIR)
        set_target_properties(${_RULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                       ${_RULE_OUTPUT_DIR})
    endif()
    if(_RULE_INCLUDES)
        cc_target_include_directories(NAME ${_RULE_NAME} INCLUDES ${_RULE_INCLUDES})
    endif()
    if(_RULE_DEFINES)
        cc_target_compile_definitions(NAME ${_RULE_NAME} DEFINES ${_RULE_DEFINES})
    endif()
    if(_RULE_COPTS)
        cc_target_compile_options(NAME ${_RULE_NAME} COPTS ${_RULE_COPTS})
    endif()
    if(_RULE_LINKOPTS)
        cc_target_link_options(NAME ${_RULE_NAME} LINKOPTS ${_RULE_LINKOPTS})
    endif()
    if(_RULE_DEPS)
        cc_target_link_libraries(NAME ${_RULE_NAME} DEPS ${_RULE_DEPS})
    endif()
    cc_target_link_libraries(NAME ${_RULE_NAME} DEPS cc-compile-flags)
    cc_add_sanitizer_flag(NAME ${_RULE_NAME})
    cc_enable_profiling(NAME ${_RULE_NAME})
endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_add_test( NAME        <name>
##                     SOURCES     [source1 [source2 ...]]
##                     HEADERS     [header1 [header2 ...]]
##                     INCLUDES    [dir1 [dir2 ...]]
##                     DEFINES     [define1 [define2 ...]]
##                     DEPENDS_ON  [dep1 [dep2 ...]]
##                     OUTPUT_DIR  [dir]
##                     OUTPUT_NAME [name]
##                     FOLDER      [name])
##
## Adds an executable target, called <name>, to be built from the given sources.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_add_test)
    cmake_parse_arguments(
        _RULE
        "" # options
        "NAME;OUTPUT_NAME" # single value argumentes
        "" # multi value argumentes
        ${ARGN})

    cc_add_executable(${ARGN})
    set(target ${_RULE_NAME})
    if(_RULE_OUTPUT_NAME)
        set(target ${_RULE_OUTPUT_NAME})
    endif()
    add_test(
        NAME ${_RULE_NAME}
        COMMAND ${target}
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endfunction()

# cmake-format: off
##------------------------------------------------------------------------------
## cc_add_library( NAME        <name>
##                 SHARED [STATIC]
##                 SOURCES     [source1 [source2 ...]]
##                 HEADERS     [header1 [header2 ...]]
##                 INCLUDES    [dir1 [dir2 ...]]
##                 DEFINES     [define1 [define2 ...]]
##                 DEPENDS_ON  [dep1 [dep2 ...]]
##                 OUTPUT_DIR  [dir]
##                 OUTPUT_NAME [name]
##                 FOLDER      [name])
##
## Adds a library target, called <name>, to be built from the given sources.
##------------------------------------------------------------------------------
# cmake-format: on
function(cc_add_library)
    cmake_parse_arguments(
        _RULE
        "SHARED;STATIC;INTERFACE" # options
        "NAME;OUTPUT_NAME" # single value argumentes
        "SRCS;HDRS;INCLUDES;DEFINES;COPTS;LINKOPTS;DEPS;TAGS" # multi value argumentes
        ${ARGN})
    # Sanity checks
    if("${_RULE_NAME}" STREQUAL "")
        message(FATAL_ERROR "cc_add_library() must be called with argument NAME <name>")
    endif()
    if(NOT _RULE_SRCS AND (_RULE_SHARED OR _RULE_STATIC))
        message(
            FATAL_ERROR "cc_add_library(NAME ${_RULE_NAME} ...) given with no sources")
    endif()
    if(_RULE_SHARED)
        set(LIB_TYPE SHARED)
    elseif(_RULE_STATIC)
        set(LIB_TYPE STATIC)
    elseif(_RULE_INTERFACE)
        set(LIB_TYPE INTERFACE)
    else()
        message(FATAL_ERROR "cc_add_library ${_RULE_NAME} invoked without library type")
    endif()

    logger(UNIT ${_RULE_NAME} LEVEL "STATUS" "Adding library ${_RULE_NAME} ${_RULE_SRCS}")
    add_library(${_RULE_NAME} ${LIB_TYPE} ${_RULE_SRCS} ${_RULE_HDRS})
    if(_RULE_OUTPUT_NAME)
        set_target_properties(${_RULE_NAME} PROPERTIES OUTPUT_NAME ${_RULE_OUTPUT_NAME})
    endif()
    if(_RULE_OUTPUT_DIR)
        set_target_properties(${_RULE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                                       ${_RULE_OUTPUT_DIR})
    endif()
    if(_RULE_INCLUDES)
        cc_target_include_directories(NAME ${_RULE_NAME} INCLUDES ${_RULE_INCLUDES})
    endif()
    if(_RULE_DEFINES)
        cc_target_compile_definitions(NAME ${_RULE_NAME} DEFINES ${_RULE_DEFINES})
    endif()
    if(_RULE_COPTS)
        cc_target_compile_options(NAME ${_RULE_NAME} COPTS ${_RULE_COPTS})
    endif()
    if(_RULE_LINKOPTS)
        cc_target_link_options(NAME ${_RULE_NAME} LINKOPTS ${_RULE_LINKOPTS})
    endif()
    if(_RULE_DEPS)
        cc_target_link_libraries(NAME ${_RULE_NAME} DEPS ${_RULE_DEPS})
    endif()
    cc_target_link_libraries(NAME ${_RULE_NAME} DEPS cc-compile-flags)
    cc_add_sanitizer_flag(NAME ${_RULE_NAME})
    cc_enable_profiling(NAME ${_RULE_NAME})

endfunction()
