add_executable(back_aux
    main.cpp
)

target_compile_options(back_aux PRIVATE -Wall -Wextra)

target_link_libraries(back_aux
    board_common
    hardware_adc
    hardware_watchdog
    hardware_pwm
)

pico_enable_stdio_uart(back_aux 0)
pico_enable_stdio_usb(back_aux 1)

pico_add_extra_outputs(back_aux)
forbid_heap(back_aux)
