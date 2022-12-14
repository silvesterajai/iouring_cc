fetchcontent_declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 9.1.0)
fetchcontent_makeavailable(fmt)
fetchcontent_getproperties(fmt)
if(NOT fmt_POPULATED)
    fetchcontent_populate(fmt)
    add_subdirectory(${fmt_SOURCE_DIR} ${fmt_BINARY_DIR})
endif()
