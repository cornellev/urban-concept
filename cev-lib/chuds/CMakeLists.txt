# header-only for now; switch to `add_library(chuds STATIC src/...)` once there
# are implementation sources
add_library(chuds INTERFACE)

target_include_directories(chuds INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_features(chuds INTERFACE cxx_std_20)
