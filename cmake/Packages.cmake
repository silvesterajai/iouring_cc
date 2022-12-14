# Enable ExternalProject CMake module
include(FetchContent)

# Google Test
include(cmake/third_party/googletest.cmake)

# Catch2 Test include(cmake/third_party/catch2.cmake)

# Dyninst include(cmake/third_party/dyninst.cmake)

# Timemory
include(cmake/third_party/timemory.cmake)

# Gperftools
find_package(Gperftools)

# Spdlog
include(cmake/third_party/spdlog.cmake)

# Spdlog
# include(cmake/third_party/fmt.cmake)

# Google benchmark
include(cmake/third_party/googlebenchmark.cmake)

# CPP benchmark include(cmake/third_party/cppbenchmark.cmake)

# liburing
include(cmake/third_party/liburing.cmake)

# abseil include(cmake/third_party/abseil.cmake)
