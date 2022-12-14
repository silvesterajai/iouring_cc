# ----------------------------------------------------------------------------------------#
#
# Google Test
#
# ----------------------------------------------------------------------------------------#
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF) # to disable abseil test
set(ABSL_ENABLE_INSTALL ON)
set(ABSL_PROPAGATE_CXX_STD ON)

fetchcontent_declare(
    absl
    GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
    GIT_TAG 20220623.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE)
fetchcontent_makeavailable(absl)
