fetchcontent_declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.10.0)
fetchcontent_makeavailable(spdlog)
fetchcontent_getproperties(spdlog)
if(NOT spdlog_POPULATED)
    fetchcontent_populate(spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()
