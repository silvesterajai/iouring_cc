# ----------------------------------------------------------------------------------------#
#
# Google Test
#
# ----------------------------------------------------------------------------------------#
fetchcontent_declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
fetchcontent_makeavailable(googletest)
set(BUILD_GMOCK
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_GTEST
    ON
    CACHE BOOL "" FORCE)
