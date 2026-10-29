add_library(board_common INTERFACE)

target_include_directories(board_common INTERFACE ${CMAKE_CURRENT_LIST_DIR}/..)

target_link_libraries(board_common INTERFACE
    chuds_mcp
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_pio
)

pico_generate_pio_header(board_common ${CMAKE_CURRENT_LIST_DIR}/rpm.pio)
