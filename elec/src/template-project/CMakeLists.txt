# to add a board:
# - copy this folder and rename the target
# - add ur sources
# - link libraries u need


add_executable(template-project
    main.cpp
)
target_compile_options(template-project PRIVATE -Wall -Wextra)

target_link_libraries(template-project
    pico_stdlib
)

pico_enable_stdio_uart(template-project 1)
pico_enable_stdio_usb(template-project 0)

# produce the .uf2 to flash onto the board
pico_add_extra_outputs(template-project)
