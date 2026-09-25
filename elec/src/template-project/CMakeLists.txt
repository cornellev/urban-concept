add_executable(template-project
    main.cpp
)

target_compile_options(template-project PRIVATE -Wall -Wextra)

# board_common (src/common) is the shared board base: pico_stdlib, spi, gpio, and the chuds mcp transport
target_link_libraries(template-project
    board_common
)

pico_enable_stdio_uart(template-project 0)
pico_enable_stdio_usb(template-project 1)

pico_add_extra_outputs(template-project)
forbid_heap(template-project)
