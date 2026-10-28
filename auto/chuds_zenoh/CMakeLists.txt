cmake_minimum_required(VERSION 3.25)

project(chuds_zenoh CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(../../cev-lib/chuds chuds)

include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM
)
FetchContent_MakeAvailable(json)

# json <-> catalog struct conversion, no zenoh, so the tests build anywhere
add_library(chuds_zenoh_convert INTERFACE)
target_include_directories(chuds_zenoh_convert INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(chuds_zenoh_convert INTERFACE chuds nlohmann_json::nlohmann_json)

# the node uses SocketCAN (Linux)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(zenohc REQUIRED)
    find_package(zenohcxx REQUIRED)
    add_executable(chuds_zenoh main.cpp)
    target_link_libraries(chuds_zenoh PRIVATE chuds_socketcan chuds_zenoh_convert zenohcxx::zenohc)
    target_compile_options(chuds_zenoh PRIVATE -Wall -Wextra)
endif()

enable_testing()
add_subdirectory(tests)
