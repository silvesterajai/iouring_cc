find_path(liburing_INCLUDE_DIR NAMES liburing.h)
find_library(liburing_LIBRARIES NAMES liburing.a liburing)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(liburing DEFAULT_MSG liburing_LIBRARIES
                                  liburing_INCLUDE_DIR)

mark_as_advanced(liburing_INCLUDE_DIR liburing_LIBRARIES)

if(liburing_FOUND AND NOT TARGET liburing::liburing)
    add_library(liburing::liburing UNKNOWN IMPORTED)
    set_target_properties(
        liburing::liburing
        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${liburing_INCLUDE_DIR}"
                   IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                   IMPORTED_LOCATION "${liburing_LIBRARIES}")
endif()
