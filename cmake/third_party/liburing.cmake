# ----------------------------------------------------------------------------------------#
#
# liburing
#
# ----------------------------------------------------------------------------------------#
set(target liburing)
set(LIBURING_PATH_PREFIX ${THIRD_PARTY_PREFIX}/liburing)
externalproject_add(
    ${target}_external
    PREFIX ${LIBURING_PATH_PREFIX}
    GIT_REPOSITORY https://github.com/axboe/liburing
    GIT_TAG 4915f2af869876d892a1f591ee2c21be21c6fc5c
    UPDATE_DISCONNECTED True
    CONFIGURE_COMMAND
        ./configure --prefix=${THIRD_PARTY_PREFIX} --libdir=${THIRD_PARTY_PREFIX}/lib
        --libdevdir=${THIRD_PARTY_PREFIX}/lib --mandir=${THIRD_PARTY_PREFIX}/man
        --includedir=${THIRD_PARTY_PREFIX}/include
    BUILD_COMMAND make
    BUILD_IN_SOURCE 1
    INSTALL_COMMAND make install
    LOG_DOWNLOAD 1
    LOG_BUILD 1)
add_library(${target} STATIC IMPORTED)
set_target_properties(${target} PROPERTIES IMPORTED_LOCATION
                                           ${THIRD_PARTY_PREFIX}/lib/liburing.a)

file(MAKE_DIRECTORY ${THIRD_PARTY_PREFIX}/include)
target_include_directories(
    ${target}
    PUBLIC
    INTERFACE ${THIRD_PARTY_PREFIX}/include)
add_dependencies(${target} ${target}_external)
