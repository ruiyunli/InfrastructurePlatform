cmake_minimum_required(VERSION 3.10)

set(TARGET_NAME qt_platform)
project(${TARGET_NAME} LANGUAGES CXX)

cmake_policy(SET CMP0091 NEW)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set variable
if(WIN32)
    set(PLATFORM_NAME "x64")
else()
    set(PLATFORM_NAME "linux")
endif(WIN32)

set(INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/install/${PLATFORM_NAME})

# Include Conan toolchain
include(${CMAKE_CURRENT_SOURCE_DIR}/build/${PLATFORM_NAME}/conan/build/generators/conan_toolchain.cmake)

# add directory
add_subdirectory(src)