cmake_minimum_required(VERSION 3.13)

project(telem_shm CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# pi only: pigpiod_if2 for the pigpio daemon, rt for shm_open, pthread for std::thread
add_executable(write_shm write_shm.cpp)
target_link_libraries(write_shm pigpiod_if2 rt pthread)
