include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.3
)
FetchContent_MakeAvailable(doctest)

add_executable(chuds-tests
    main.cpp
    test_ids.cpp
    test_message.cpp
    test_transport.cpp
)
target_link_libraries(chuds-tests PRIVATE chuds doctest::doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(chuds-tests)
