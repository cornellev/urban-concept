cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-sdk)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(urban_concept_elec C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

pico_sdk_init()

# every subdirectory of src/ is a separate firmware project

file(GLOB entries RELATIVE ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/src/*)

foreach(entry ${entries})
    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/src/${entry}/CMakeLists.txt)
        add_subdirectory(src/${entry})
    endif()
endforeach()
