cmake_minimum_required(VERSION 3.15)
project(LoginBackend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 安装目录
if(WIN32)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/x64")
elseif(UNIX)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/linux")
endif()

set(TARGET_NAME server)

# 添加include目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# Windows MSVC编译器使用UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 查找OpenSSL
find_package(OpenSSL REQUIRED)

# 添加可执行文件
add_executable(${TARGET_NAME}
    src/main.cpp
)

# 链接OpenSSL库
target_link_libraries(${TARGET_NAME}
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Windows特定设置
if(WIN32)
    target_link_libraries(${TARGET_NAME} ws2_32)

    # 设置 Visual Studio 启动项目
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_STARTUP_PROJECT TRUE)

    # 设置调试属性
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_COMMAND "${INSTALL_DIR}/${TARGET_NAME}.exe")
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${INSTALL_DIR}")
endif()



# 安装server可执行文件
install(TARGETS ${TARGET_NAME}
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
