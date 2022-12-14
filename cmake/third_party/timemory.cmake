# ----------------------------------------------------------------------------------------#
#
# Timemory
#
# ----------------------------------------------------------------------------------------#
if(USE_TIMEMORY)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
    set(TIMEMORY_USE_PAPI ON)
    set(TIMEMORY_USE_GOTCHA OFF)
    set(TIMEMORY_BUILD_MALLOCP_LIBRARY OFF)
    set(TIMEMORY_BUILD_TIMEM ON)
    set(TIMEMORY_BUILD_TOOLS ON)
    set(TIMEMORY_INSTALL_HEADERS ON)
    set(TIMEMORY_USE_DYNINST OFF)
    set(TIMEMORY_USE_LIBUNWIND ON)
    set(CMAKE_INSTALL_PREFIX /usr/local/)
    fetchcontent_declare(
        timemory
        GIT_REPOSITORY https://github.com/NERSC/timemory.git
        GIT_TAG v3.2.3)

    fetchcontent_makeavailable(timemory)

    set(timemory_FIND_COMPONENTS_INTERFACE timemory-extern-interface)
    find_package(
        timemory REQUIRED
        COMPONENTS headers compile-options compiler-instrument
        OPTIONAL_COMPONENTS cxx papi)

    if(NOT timemory_FOUND OR NOT TARGET timemory::timemory-compiler-instrument)
        message(FATAL_ERROR "Aooooooo")
    endif()
endif()
