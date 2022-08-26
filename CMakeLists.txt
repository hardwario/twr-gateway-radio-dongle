cmake_minimum_required(VERSION 3.20.0)

# Setup project name and languages
project(firmware LANGUAGES C ASM)

add_subdirectory(sdk)

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC TWR_SCHEDULER_MAX_TASKS=64)
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC TWR_RADIO_MAX_DEVICES=32)

# If you need to add some source files to the project add them to the "src" folder and update CMakeLists there
add_subdirectory(src)
