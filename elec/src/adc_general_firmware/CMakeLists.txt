add_executable(adc_general_firmware
    spi_slave.c
)

target_compile_options(adc_general_firmware PRIVATE -Wall -Wextra)

target_include_directories(adc_general_firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(adc_general_firmware
    pico_stdlib
    hardware_adc
    hardware_dma
    hardware_spi
    hardware_pio
)

pico_enable_stdio_uart(adc_general_firmware 0)
pico_enable_stdio_usb(adc_general_firmware 1)

pico_add_extra_outputs(adc_general_firmware)
