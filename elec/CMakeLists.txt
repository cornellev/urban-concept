cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/../vendor/pico-sdk)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(urban_concept_elec C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../cev-lib/RP2040_MCP251863_Driver rp2040_mcp251863)

# firmware inventory; add a board with an explicit line
add_subdirectory(src/adc_general_firmware)
add_subdirectory(src/can_firmware)
add_subdirectory(src/rpm_firmware)
add_subdirectory(src/template-project)
