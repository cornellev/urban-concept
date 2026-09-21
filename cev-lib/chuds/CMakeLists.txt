cmake_minimum_required(VERSION 3.21)

project(chuds CXX)

# core library
add_library(chuds INTERFACE)
target_include_directories(chuds INTERFACE ${CMAKE_CURRENT_LIST_DIR}/..)
target_compile_features(chuds INTERFACE cxx_std_20)

# opt-in Linux SocketCAN adapter
add_library(chuds_socketcan INTERFACE)
target_link_libraries(chuds_socketcan INTERFACE chuds)

# opt-in MCP251863 adapter for RP2040, needs the chip driver target
add_library(chuds_mcp INTERFACE)
target_link_libraries(chuds_mcp INTERFACE chuds)
if(TARGET rp2040_mcp251863)
    target_link_libraries(chuds_mcp INTERFACE rp2040_mcp251863)
endif()

# only build tests and examples when not being used as a library
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    enable_testing()
    add_subdirectory(tests)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_subdirectory(examples)
    endif()
endif()
