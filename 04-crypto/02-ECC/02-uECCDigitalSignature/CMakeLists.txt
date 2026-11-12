cmake_minimum_required(VERSION 3.15)
project(EccDigitalSignature)

# uECC include directory
if(WIN32)
    set(UECC_INC_DIR "${CMAKE_SOURCE_DIR}/3rdparty/micro-ecc-1.1/win32/include")
    set(UECC_LIB_DIR "${CMAKE_SOURCE_DIR}/3rdparty/micro-ecc-1.1/win32/lib")
else()
    set(UECC_INC_DIR "${CMAKE_SOURCE_DIR}/3rdparty/micro-ecc-1.1/linux/include")
    set(UECC_LIB_DIR "${CMAKE_SOURCE_DIR}/3rdparty/micro-ecc-1.1/linux/lib")
endif()
include_directories(${UECC_INC_DIR})

# Windows MSVC编译器使用UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

set(TARGET_NAME EccDigitalSignature)

# 添加可执行文件
add_executable(${TARGET_NAME}
    main.c
)

# 链接 uECC 库
if(WIN32)
    find_library(UECC_LIB micro-ecc PATHS ${UECC_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(${TARGET_NAME} ${UECC_LIB} bcrypt)
else()
    find_library(UECC_LIB micro-ecc PATHS ${UECC_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(${TARGET_NAME} ${UECC_LIB} bcrypt)
endif()

# Windows特定设置
if(WIN32)
    # 设置 Visual Studio 启动项目
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TARGET_NAME})
endif()
