cmake_minimum_required(VERSION 3.15)
project(socket C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 安装目录
if(WIN32)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/x64")
elseif(UNIX)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/linux")
endif()

# Windows MSVC编译器使用UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 两个独立的可执行文件
add_executable(server main-server.c)
add_executable(client main-client.c)

if(WIN32)
    # Windows 需要链接 Winsock 库
    target_link_libraries(server PRIVATE ws2_32)
    target_link_libraries(client PRIVATE ws2_32)

    # 设置 Visual Studio 启动项目为 server
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT server)

    # 设置两个项目的调试属性
    foreach(target_name server client)
        set_property(TARGET ${target_name} PROPERTY VS_DEBUGGER_COMMAND "${INSTALL_DIR}/${target_name}.exe")
        set_property(TARGET ${target_name} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${INSTALL_DIR}")
    endforeach()
else()
    # Linux 下后台输入线程需要 pthread
    find_package(Threads REQUIRED)
    target_link_libraries(server PRIVATE Threads::Threads)
    target_link_libraries(client PRIVATE Threads::Threads)
endif()

# 安装可执行文件
install(TARGETS server client
    RUNTIME DESTINATION ${INSTALL_DIR}
)
