# only include this once
include_guard(GLOBAL)

set(BUILD_TESTING
    ON
    CACHE BOOL "Build testing program" FORCE)

if(BUILD_TESTING)
    set(BUILD_GOOGLE_TEST ON)
    find_package(Git REQUIRED)
endif()

# Default to release
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE
        Release
        CACHE STRING "Build type" FORCE)
endif()

# Ensure that comple command json file is created
set(CMAKE_EXPORT_COMPILE_COMMANDS
    ON
    CACHE INTERNAL "")

option(SANITIZE_ADDRESS "Enable address sanitizer" OFF)
option(SANITIZE_MEMORY "Enable memory sanitizer" OFF)
option(SANITIZE_THREAD "Enable thread sanitizer" OFF)
option(SANITIZE_UNDEFINED "Enable undefined sanitizer" OFF)

option(USE_TIMEMORY "Enable timemory support" OFF)
option(USE_PAPI "Enable PAPI support" OFF)
option(USE_COMPILER_INSTRUMENTATION "Enable compiler instrumentation" OFF)
option(CODE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_BENCHMARK "Enable benchmark" OFF)

# Set CXX standard to 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set custom cmake module path
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckIncludeFiles)
include(ExternalProject)

include_directories(${CMAKE_SOURCE_DIR})
# Third party path
set(THIRD_PARTY_PREFIX ${CMAKE_BINARY_DIR}/third_party)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INSTALL_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE STRING
"Installation directory for include files, a relative path that "
"will be joined with ${CMAKE_INSTALL_PREFIX} or an absolute path.")
