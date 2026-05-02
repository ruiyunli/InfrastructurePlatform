# Find Qt5 components
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

message(STATUS "Qt5_FOUND=${Qt5_FOUND}")
message(STATUS "Qt5_INCLUDE_DIRS=${Qt5_INCLUDE_DIRS}")

# Add executable
add_executable(${TARGET_NAME} 
    main.cpp
    plugin_import.cpp
)

target_include_directories(${TARGET_NAME} SYSTEM PRIVATE ${Qt5_INCLUDE_DIRS})

# Link Qt libraries
target_link_libraries(${TARGET_NAME} Qt5::Core Qt5::Widgets Qt5::QWindowsIntegrationPlugin)

# Import Qt plugins for static build
qt5_import_plugins(${TARGET_NAME} INCLUDE Qt5::QWindowsIntegrationPlugin)

# INSTALL exe
install(TARGETS ${TARGET_NAME}
    RUNTIME DESTINATION ${INSTALL_DIR}
    LIBRARY DESTINATION ${INSTALL_DIR}
    ARCHIVE DESTINATION ${INSTALL_DIR}
)


# 依赖库安装
#install(RUNTIME_DEPENDENCIES
#    TARGETS ${TARGET_NAME}
#    DIRECTORIES ${CONAN_QT_BIN_DIR} 
#    DESTINATION ${INSTALL_DIR}
#)