cmake_minimum_required(VERSION 3.15)
project(LoginBackend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# 安装配置
if(WIN32)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/x64")
elseif(UNIX)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/linux")
endif()

# 安装server可执行文件
install(TARGETS server
    RUNTIME DESTINATION ${INSTALL_DIR}
)

# 安装 backgrounds 目录
install(DIRECTORY ${CMAKE_SOURCE_DIR}/release/backgrounds
    DESTINATION ${INSTALL_DIR}
)

# Windows: 安装 libcrypto DLL（根据当前配置）
if(WIN32)
    install(FILES "${CMAKE_BINARY_DIR}/$<CONFIG>/libcrypto-3-x64.dll"
        DESTINATION ${INSTALL_DIR}
    )
endif()
