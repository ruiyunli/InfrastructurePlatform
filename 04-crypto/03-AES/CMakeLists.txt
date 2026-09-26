cmake_minimum_required(VERSION 3.15)
project(AESEncodeDecode)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 安装目录
if(WIN32)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/x64")
elseif(UNIX)
    set(INSTALL_DIR "${CMAKE_SOURCE_DIR}/install/linux")
endif()

set(TARGET_NAME AESEncodeDecode)

# mbedtls include directory
if(WIN32)
    set(MBEDTLS_INC_DIR "${CMAKE_SOURCE_DIR}/3rdparty/mbedtls-4.1.0/win32/include")
    set(MBEDTLS_LIB_DIR "${CMAKE_SOURCE_DIR}/3rdparty/mbedtls-4.1.0/win32/lib")
else()
    # Linux: assume mbedtls built and installed to 3rdparty/mbedtls-4.1.0/linux/
    set(MBEDTLS_INC_DIR "${CMAKE_SOURCE_DIR}/3rdparty/mbedtls-4.1.0/linux/include")
    set(MBEDTLS_LIB_DIR "${CMAKE_SOURCE_DIR}/3rdparty/mbedtls-4.1.0/linux/lib")
endif()
include_directories(${MBEDTLS_INC_DIR})

# Windows MSVC编译器使用UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 添加可执行文件
add_executable(${TARGET_NAME}
    main.c
)

# mbedtls libraries (PSA Crypto API)
if(WIN32)
    find_library(MBEDTLS_LIB mbedtls PATHS ${MBEDTLS_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
    find_library(TFPSACRYPTO_LIB tfpsacrypto PATHS ${MBEDTLS_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
    find_library(MBEDX509_LIB mbedx509 PATHS ${MBEDTLS_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(${TARGET_NAME} ${MBEDTLS_LIB} ${TFPSACRYPTO_LIB} ${MBEDX509_LIB} bcrypt)
else()
    find_library(MBEDTLS_LIB mbedtls PATHS ${MBEDTLS_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
    find_library(TFPSACRYPTO_LIB tfpsacrypto PATHS ${MBEDTLS_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(${TARGET_NAME} ${MBEDTLS_LIB} ${TFPSACRYPTO_LIB} pthread m)
endif()

# Windows特定设置
if(WIN32)
    # 设置 Visual Studio 启动项目
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})

    # 设置调试属性
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_COMMAND "${INSTALL_DIR}/${TARGET_NAME}.exe")
    set_property(TARGET ${TARGET_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${INSTALL_DIR}")
endif()



# 安装可执行文件
install(TARGETS ${TARGET_NAME}    RUNTIME DESTINATION ${INSTALL_DIR})

install(DIRECTORY keys                    DESTINATION ${INSTALL_DIR}  FILES_MATCHING PATTERN "*.pem")
