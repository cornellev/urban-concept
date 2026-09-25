add_executable(joulemeter
    main.cpp
)

target_compile_options(joulemeter PRIVATE -Wall -Wextra)

target_link_libraries(joulemeter
    board_common
    hardware_adc
)

pico_enable_stdio_uart(joulemeter 0)
pico_enable_stdio_usb(joulemeter 1)

pico_add_extra_outputs(joulemeter)
forbid_heap(joulemeter)
