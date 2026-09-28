# prints the ip link arguments for the bus settings in frame.hpp, used by can0-up.service
add_executable(can_link_args can_link_args.cpp)
target_link_libraries(can_link_args PRIVATE chuds)
target_compile_options(can_link_args PRIVATE -Wall -Wextra)
