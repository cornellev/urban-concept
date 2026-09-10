add_executable(can_firmware
    can_firmware.cpp
)

target_compile_options(can_firmware PRIVATE -Wall -Wextra)

target_link_libraries(can_firmware
    rp2040_mcp251863
    pico_stdlib
    hardware_spi
)

pico_enable_stdio_uart(can_firmware 0)
pico_enable_stdio_usb(can_firmware 1)

pico_add_extra_outputs(can_firmware)
