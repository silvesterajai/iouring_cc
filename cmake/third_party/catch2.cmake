# ----------------------------------------------------------------------------------------#
#
# Catch2 Test
#
# ----------------------------------------------------------------------------------------#
if(CUCKOO_BUILD_TESTING)
    fetchcontent_declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.0.1 # or a later release
        )

    fetchcontent_makeavailable(Catch2)
endif()
