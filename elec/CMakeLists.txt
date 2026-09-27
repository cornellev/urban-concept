cmake_minimum_required(VERSION 3.21)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/../vendor/pico-sdk)
include(${CMAKE_CURRENT_LIST_DIR}/cmake/pinned_tools.cmake)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(urban_concept_elec C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../cev-lib/RP2040_MCP251863_Driver rp2040_mcp251863)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../cev-lib/chuds chuds)

add_subdirectory(src/common)

# a board built from its folder's main.cpp, named after the folder so `just flash <folder>` finds it
# arguments are extra libraries on top of board_common
function(add_board)
    get_filename_component(board ${CMAKE_CURRENT_SOURCE_DIR} NAME)
    add_executable(${board} main.cpp)
    target_compile_options(${board} PRIVATE -Wall -Wextra)
    target_link_libraries(${board} PRIVATE board_common ${ARGN})
    pico_enable_stdio_uart(${board} 0)
    pico_enable_stdio_usb(${board} 1)
    pico_add_extra_outputs(${board})
    # fail the build if the board links the heap allocator
    add_custom_command(TARGET ${board} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DNM=${CMAKE_NM} -DELF=$<TARGET_FILE:${board}>
                -P ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/cmake/forbid_heap.cmake)
endfunction()

# firmware inventory; add a board with an explicit line
add_subdirectory(src/front_aux)
add_subdirectory(src/back_aux)
add_subdirectory(src/joulemeter)
add_subdirectory(src/template-project)
