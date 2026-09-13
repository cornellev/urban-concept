# standalone harness for building the driver plus its examples on their own
# elec builds the driver as a component instead and never enters this file

cmake_minimum_required(VERSION 3.13)

get_filename_component(CEV_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH "${CEV_REPO_ROOT}/vendor/pico-sdk" CACHE PATH "path to pico-sdk")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rp2040_mcp251863_examples C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_subdirectory(.. ${CMAKE_CURRENT_BINARY_DIR}/driver)

foreach(example canfd_tx_example canfd_rx_example)
    add_executable(${example} ${example}.cpp)
    target_link_libraries(${example} PRIVATE rp2040_mcp251863)
    pico_add_extra_outputs(${example})
endforeach()
