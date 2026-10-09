# Project Qt source/header files
file(GLOB_RECURSE QOBJECTS_MODULE_SRC_FILES qobjects/*.cpp)
file(GLOB_RECURSE QOBJECTS_MODULE_HEADER_FILES qobjects/*.hpp)

# Project module source files
file(GLOB_RECURSE LOGGING_MODULE_SRC_FILES logging/*.cppm)

# Third-party module source files
file(GLOB_RECURSE MAGIC_ENUM_MODULE_SRC_FILES "${magic_enum_SOURCE_DIR}/module/*.cppm")

find_package(EnTT REQUIRED)
find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickShapes CanvasPainter)
qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(qml/)
add_subdirectory(third_party/)
add_subdirectory(json/)
add_subdirectory(foundation/)
add_subdirectory(core/)
add_subdirectory(ecs/)
add_subdirectory(imagery/)
add_subdirectory(map/)

# Add to build
add_executable_target(
    mts
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    SOURCE_FILES
        main.cpp
        app/application.cpp
    MODULE_FILES
        ${LOGGING_MODULE_SRC_FILES}
)

add_qml_module_target(
    mts
    URI Application
    VERSION 1.0
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    QML_SOURCE_FILES
        ${QOBJECTS_MODULE_SRC_FILES}
        ${QOBJECTS_MODULE_HEADER_FILES}
    QML_MODULE_FILES
        qml/Main.qml
)

target_include_directories(
    mts
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        "${CMAKE_CURRENT_SOURCE_DIR}/qobjects/"
        "${CMAKE_CURRENT_SOURCE_DIR}/app/"
)

target_link_libraries(
    mts
    PUBLIC
        spdlog::spdlog
        STDEXEC::stdexec
        Qt6::Quick
        Qt6::CanvasPainter
        EnTT::EnTT
        lgc::langgraph
        LibMagicEnum
        LibCore
        LibEcs
        LibImagery
        LibMap
    PRIVATE
        LibQml
)
