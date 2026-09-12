cmake_minimum_required(VERSION 3.28)

project(chuds-tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.5.3
)
FetchContent_MakeAvailable(doctest)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. chuds)

add_executable(chuds-tests
    main.cpp
    test_ids.cpp
)
target_link_libraries(chuds-tests PRIVATE chuds doctest::doctest)

enable_testing()
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(chuds-tests)
