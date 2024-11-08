cmake_minimum_required(VERSION 3.5)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(BUILD_TEST OFF)
set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)

add_subdirectory(external/rpi_ws281x)
add_subdirectory(external/yaml-cpp)

project(OpenGLED VERSION 0.1.0)

add_executable(open_gled src/main.cpp)
target_include_directories(open_gled PRIVATE include)

target_include_directories(open_gled PRIVATE external/rpi_ws281x)
target_include_directories(open_gled PRIVATE external/yaml-cpp/include)

target_link_libraries(open_gled PRIVATE ws2811)
target_link_libraries(open_gled PRIVATE yaml-cpp)
