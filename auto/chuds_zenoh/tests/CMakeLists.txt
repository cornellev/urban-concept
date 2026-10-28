include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG 2d0a9359a60c51affe2a9bebb1be1dca47868151 # v2.5.3
)
FetchContent_MakeAvailable(doctest)

add_executable(chuds-zenoh-tests
    main.cpp
    test_convert.cpp
)
target_link_libraries(chuds-zenoh-tests PRIVATE chuds_zenoh_convert doctest::doctest)
target_compile_options(chuds-zenoh-tests PRIVATE -Wall -Wextra -Wconversion)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(chuds-zenoh-tests)
