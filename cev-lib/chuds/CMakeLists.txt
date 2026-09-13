cmake_minimum_required(VERSION 3.28)

project(chuds CXX)

# header-only for now; switch to `add_library(chuds STATIC src/...)` once there
# are implementation sources
add_library(chuds INTERFACE)

target_include_directories(chuds INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_features(chuds INTERFACE cxx_std_20)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    enable_testing()
    add_subdirectory(tests)
endif()
