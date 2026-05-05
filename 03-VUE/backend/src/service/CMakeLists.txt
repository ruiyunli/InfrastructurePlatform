cmake_minimum_required(VERSION 3.15)


# Service 模块 - OBJECT 库
add_library(service_obj OBJECT
    backgroudmanager.cpp
    authservice.cpp
)

# 设置 include 目录
target_include_directories(service_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接 OpenSSL（提供头文件路径）
target_link_libraries(service_obj PUBLIC OpenSSL::SSL OpenSSL::Crypto)
