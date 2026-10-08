# Project Qt source/header files
file(GLOB_RECURSE QOBJECTS_MODULE_SRC_FILES qobjects/*.cpp)
file(GLOB_RECURSE QOBJECTS_MODULE_HEADER_FILES qobjects/*.hpp)

# Project module source files
file(GLOB_RECURSE LOGGING_MODULE_SRC_FILES logging/*.cppm)

# Third-party module source files
file(GLOB_RECURSE MAGIC_ENUM_MODULE_SRC_FILES "${magic_enum_SOURCE_DIR}/module/*.cppm")

find_package(EnTT REQUIRED)
find_package(Qt6 6.11 REQUIRED COMPONENTS Quick CanvasPainter)
qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(qml/)
add_subdirectory(core/)
add_subdirectory(ecs/)
add_subdirectory(imagery/)
add_subdirectory(map/)

# Add to build
add_executable_target(
    mts
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${magic_enum_SOURCE_DIR}/module"
    SOURCE_FILES
        main.cpp
    MODULE_FILES
        ${LOGGING_MODULE_SRC_FILES}
    THIRD_PARTY_MODULE_FILES
        ${MAGIC_ENUM_MODULE_SRC_FILES}
)

set_source_files_properties(
    "${PROJECT_SOURCE_DIR}/everon.png"
    PROPERTIES
        QT_RESOURCE_ALIAS "everon.png"
)

qt_add_resources(
    mts
    "assets"
    PREFIX
        "/assets"
    FILES
        "${PROJECT_SOURCE_DIR}/everon.png"
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
        "${magic_enum_SOURCE_DIR}/include/"
        "${CMAKE_CURRENT_SOURCE_DIR}/qobjects/"
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
        LibCore
        LibEcs
        LibImagery
        LibMap
    PRIVATE
        LibQml
)
