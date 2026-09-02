add_executable(rpm_firmware
    rpm_firmware.c
)

target_compile_options(rpm_firmware PRIVATE -Wall -Wextra)

target_include_directories(rpm_firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(rpm_firmware
    pico_stdlib
    hardware_spi
    hardware_dma
)

pico_enable_stdio_uart(rpm_firmware 0)
pico_enable_stdio_usb(rpm_firmware 1)

pico_add_extra_outputs(rpm_firmware)
