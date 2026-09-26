cmake_minimum_required(VERSION 3.15)
project(HelloWorld)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 安装目录
if(WIN32)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/x64")
elseif(UNIX)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/linux")
endif()

set(TARGET_NAME HelloWorld)

# Windows MSVC编译器使用UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 添加可执行文件
add_executable(${TARGET_NAME}
    main.c
)

# Windows特定设置
if(WIN32)
    # 设置 Visual Studio 启动项目
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})

    # 设置调试属性
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_COMMAND "${INSTALL_DIR}/${TARGET_NAME}.exe")
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${INSTALL_DIR}")
endif()



# 安装可执行文件
install(TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION ${INSTALL_DIR}
)
