# ----------------------------------------------------------------------------------------#
#
# Timemory
#
# ----------------------------------------------------------------------------------------#
if(USE_TIMEMORY)
    set(STERILE_BUILD OFF)
    set(CMAKE_INSTALL_PREFIX /usr/local/)
    fetchcontent_declare(
        dyninst
        GIT_REPOSITORY https://github.com/dyninst/dyninst
        GIT_TAG v12.2.0)

    fetchcontent_makeavailable(dyninst)
endif()
