if(NOT ENABLE_BENCHMARK)
    return()
endif()
# Disable the Google Benchmark requirement on Google Test
set(BENCHMARK_ENABLE_TESTING NO)

fetchcontent_declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.7.0)

fetchcontent_makeavailable(googlebenchmark)
