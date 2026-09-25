cmake_minimum_required(VERSION 3.21)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/../vendor/pico-sdk)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(urban_concept_elec C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# help find_package(picotool) across install locations
# the cmake variable is searched before the env var, so probe brew only when no shell set the env
if(CMAKE_HOST_WIN32)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/.tools/picotool")
endif()
if(NOT DEFINED ENV{CMAKE_PREFIX_PATH})
    execute_process(COMMAND brew --prefix
        OUTPUT_VARIABLE BREW_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(BREW_PREFIX)
        list(APPEND CMAKE_PREFIX_PATH "${BREW_PREFIX}")
    endif()
endif()

pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../cev-lib/RP2040_MCP251863_Driver rp2040_mcp251863)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../cev-lib/chuds chuds)

add_subdirectory(src/common)

# fail the firmware build if target links the heap allocator
function(forbid_heap target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DELF=$<TARGET_FILE:${target}>
                -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cmake/forbid_heap.cmake)
endfunction()

# firmware inventory; add a board with an explicit line
add_subdirectory(src/front_aux)
add_subdirectory(src/back_aux)
add_subdirectory(src/joulemeter)
add_subdirectory(src/template-project)
