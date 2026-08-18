cmake_minimum_required(VERSION 3.15)

# Router 模块 - OBJECT 库
add_library(router_obj OBJECT
    ApiRouter.cpp
)

target_include_directories(router_obj PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(router_obj PUBLIC OpenSSL::SSL OpenSSL::Crypto)
