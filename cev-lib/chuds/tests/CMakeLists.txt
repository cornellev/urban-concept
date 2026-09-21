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
    test_io.cpp
)
target_link_libraries(chuds-tests PRIVATE chuds doctest::doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(chuds-tests)

# the socketcan adapter is Linux-only
# test needs a CAN interface (skips without one)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(chuds-socketcan-tests
        main.cpp
        test_socketcan.cpp
    )
    target_link_libraries(chuds-socketcan-tests PRIVATE chuds_socketcan doctest::doctest)
    target_compile_options(chuds-socketcan-tests PRIVATE -Wall -Wextra)
    doctest_discover_tests(chuds-socketcan-tests)
endif()
