cmake_minimum_required(VERSION 3.15)
project(LoginBackend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置vcpkg工具链文件
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

# 添加include目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# Windows MSVC编译器使用UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 查找OpenSSL
find_package(OpenSSL REQUIRED)

# 添加可执行文件
add_executable(server
    src/main.cpp
)

# 链接OpenSSL库
target_link_libraries(server
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Windows特定设置
if(WIN32)
    target_link_libraries(server ws2_32)
endif()
