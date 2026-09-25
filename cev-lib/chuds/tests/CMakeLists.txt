include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG 2d0a9359a60c51affe2a9bebb1be1dca47868151 # v2.5.3
)
FetchContent_MakeAvailable(doctest)

add_executable(chuds-tests
    main.cpp
    test_ids.cpp
    test_message.cpp
    test_transport.cpp
    test_io.cpp
    test_wire_format.cpp
)
target_link_libraries(chuds-tests PRIVATE chuds doctest::doctest)
target_compile_options(chuds-tests PRIVATE -Wall -Wextra -Wconversion)

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
    # the tests share one interface, so they must not run in parallel
    doctest_discover_tests(chuds-socketcan-tests PROPERTIES RESOURCE_LOCK vcan)
endif()
