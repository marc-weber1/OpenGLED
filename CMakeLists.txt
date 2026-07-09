cmake_minimum_required(VERSION 3.5)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Build all vendored libraries static so the installed open_gled binary is
# self-contained (buildroot passes -DBUILD_SHARED_LIBS=ON, and untyped
# add_library() calls in the subprojects follow it)
set(BUILD_SHARED_LIBS OFF)

set(YAML_CPP_BUILD_TESTS OFF)
set(YAML_CPP_BUILD_TOOLS OFF)
add_subdirectory(external/yaml-cpp)

set(BUILD_TEST OFF)
add_subdirectory(external/rpi_ws281x)

set(IIR1_BUILD_TESTING OFF)
set(IIR1_BUILD_DEMO OFF)
set(IIR1_INSTALL_STATIC OFF)
add_subdirectory (external/iir1)

project(OpenGLED VERSION 0.1.0)

add_executable(open_gled src/main.cpp src/OpenGLEDConfig.cpp src/RaspiHeadlessOpenGLContext.cpp src/RotaryEncoder.cpp src/Shader.cpp src/StartupDiagnostics.cpp external/ALSA.CPP/ALSADevices.cpp external/argspp/src/args.cpp)
target_include_directories(open_gled PRIVATE include)

target_include_directories(open_gled PRIVATE external/rpi_ws281x)
target_include_directories(open_gled PRIVATE external/yaml-cpp/include)
target_include_directories(open_gled PRIVATE external/ALSA.CPP)
target_include_directories(open_gled PRIVATE external/iir1/iir1)
target_include_directories(open_gled PRIVATE external/argspp/src)

target_link_libraries(open_gled PRIVATE ws2811)
target_link_libraries(open_gled PRIVATE yaml-cpp)
target_link_libraries(open_gled PRIVATE EGL GLESv2 gbm)
target_link_libraries(open_gled PRIVATE asound)
target_link_libraries(open_gled PRIVATE iir::iir_static)
