cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 传输后端：默认纯 TCP，可选 mbedTLS
option(NET_ENABLE_TLS "Enable TLS backend (requires mbedTLS in 3rdparty)" OFF)

if(NET_ENABLE_TLS)
    set(NET_VARIANT "tls")
    set(NET_BACKEND_SOURCE net-tls.c)
else()
    set(NET_VARIANT "notls")
    set(NET_BACKEND_SOURCE net-notls.c)
endif()

project(socket-tls-${NET_VARIANT} C)


# 安装目录按变体隔离，避免两种构建互相覆盖
if(WIN32)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/x64/${NET_VARIANT}")
elseif(UNIX)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/linux/${NET_VARIANT}")
endif()

# Windows MSVC编译器使用UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 公共网络模块：公共部分 + 选定的传输后端
add_library(net STATIC
    net-common.c
    ${NET_BACKEND_SOURCE}
    net-backend.h
    net-common.h
    net-internal.h
)

# 两个独立的可执行文件
add_executable(server main-server.c)
add_executable(client main-client.c)

target_link_libraries(server PRIVATE net)
target_link_libraries(client PRIVATE net)

if(WIN32)
    # Windows 需要链接 Winsock 库
    target_link_libraries(net PUBLIC ws2_32)

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
    target_link_libraries(net PUBLIC Threads::Threads)
endif()

# mbedTLS：由 configure.py 从 SVN 下载到 3rdparty 目录
if(NET_ENABLE_TLS)
    target_compile_definitions(net PUBLIC NET_ENABLE_TLS=1)

    if(WIN32)
        set(MBEDTLS_ROOT "${CMAKE_SOURCE_DIR}/3rdparty/mbedtls-4.1.0/win32")
    else()
        set(MBEDTLS_ROOT "${CMAKE_SOURCE_DIR}/3rdparty/mbedtls-4.1.0/linux")
    endif()

    target_include_directories(net PUBLIC "${MBEDTLS_ROOT}/include")

    find_library(MBEDTLS_LIB mbedtls
        PATHS "${MBEDTLS_ROOT}/lib" NO_DEFAULT_PATH)
    find_library(TFPSACRYPTO_LIB tfpsacrypto
        PATHS "${MBEDTLS_ROOT}/lib" NO_DEFAULT_PATH)
    find_library(MBEDX509_LIB mbedx509
        PATHS "${MBEDTLS_ROOT}/lib" NO_DEFAULT_PATH)

    if(NOT MBEDTLS_LIB OR NOT TFPSACRYPTO_LIB OR NOT MBEDX509_LIB)
        message(FATAL_ERROR
            "mbedTLS not found in ${MBEDTLS_ROOT}. 请先运行: python configure.py -tls")
    endif()

    if(WIN32)
        # bcrypt 供 tfpsacrypto 的随机数实现使用
        target_link_libraries(net PUBLIC
            ${MBEDTLS_LIB} ${TFPSACRYPTO_LIB} ${MBEDX509_LIB} bcrypt)
    else()
        target_link_libraries(net PUBLIC
            ${MBEDTLS_LIB} ${TFPSACRYPTO_LIB} ${MBEDX509_LIB} pthread m)
    endif()
endif()

# 安装可执行文件
install(TARGETS server client
    RUNTIME DESTINATION ${INSTALL_DIR}
)

# 证书目录随可执行文件一起部署到 install/<platform>/<variant>/certs
if(NET_ENABLE_TLS)
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/certs/"
        DESTINATION "${INSTALL_DIR}/certs"
        PATTERN "*.md" EXCLUDE
    )
endif()
