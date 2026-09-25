add_executable(front_aux
    main.cpp
)

target_compile_options(front_aux PRIVATE -Wall -Wextra)

target_link_libraries(front_aux
    board_common
    hardware_adc
    hardware_watchdog
)

pico_enable_stdio_uart(front_aux 0)
pico_enable_stdio_usb(front_aux 1)

pico_add_extra_outputs(front_aux)
forbid_heap(front_aux)
