cmake_minimum_required(VERSION 3.8...3.25)

# Fallback for using newer policies on CMake <3.12.
if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND CMAKE_CURRENT_SOURCE_DIR STREQUAL
                                                  CMAKE_SOURCE_DIR)
    set(MSG "")
    message(STATUS "Warning! Building from the source directory is not recommended")
    message(STATUS "If unintented, please remove 'CMakeCache.txt' and 'CMakeFiles'")
    message(STATUS "and build from a separate directory")
    message(FATAL_ERROR "In-source build")
endif()

project(
    iouring_cc
    LANGUAGES C CXX
    VERSION 0.0.1)

include(cmake/ProjectSetup.cmake)
include(GNUInstallDirs)

enable_testing()
include(cmake/Macros.cmake)
include(cmake/ProjectSetup.cmake)
include(cmake/Packages.cmake)
include(cmake/Targets.cmake)
include(cmake/code-coverage.cmake)

add_subdirectory(timemory_interface)
add_subdirectory(iouring)
add_subdirectory(test)
add_subdirectory(examples)
