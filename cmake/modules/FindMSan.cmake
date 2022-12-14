set(MSan_LIB_NAME MSan)

find_library(
    MSan_LIBRARY
    NAMES libmsan.so libmsan.so.6 libmsan.so.5
    PATHS ${SANITIZER_PATH} /usr/lib64 /usr/lib /usr/local/lib64 /usr/local/lib
          ${CMAKE_PREFIX_PATH}/lib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MSan DEFAULT_MSG MSan_LIBRARY)

mark_as_advanced(MSan_LIBRARY MSan_LIB_NAME)
