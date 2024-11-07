cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(BUILD_TEST OFF)

add_subdirectory(external/rpi_ws281x)

include_directories(external/rpi_ws281x)

project(OpenGLED VERSION 0.1.0)

add_executable(open_gled main.cpp)

target_link_libraries(open_gled PRIVATE ws2811)
