# Project Qt source/header files
file(GLOB_RECURSE QOBJECTS_MODULE_SRC_FILES CONFIGURE_DEPENDS qobjects/*.cpp)
file(GLOB_RECURSE QOBJECTS_MODULE_HEADER_FILES CONFIGURE_DEPENDS qobjects/*.hpp)

# Project module source files
file(GLOB_RECURSE LOGGING_MODULE_SRC_FILES CONFIGURE_DEPENDS logging/*.cppm)

find_package(EnTT REQUIRED)
find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickShapes CanvasPainter)
qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(qml/)
add_subdirectory(third_party/)
add_subdirectory(json/)
add_subdirectory(core/)
add_subdirectory(platform/)
add_subdirectory(agent/)
add_subdirectory(config/)
add_subdirectory(persistence/)
add_subdirectory(provider/)
add_subdirectory(events/)
add_subdirectory(ecs/)
add_subdirectory(render2d/)
add_subdirectory(imagery/)
add_subdirectory(map/)
add_subdirectory(orchestration/)

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
        LibPlatform
        LibAgent
        LibConfig
        LibPersistence
        LibProvider
        LibEvents
        LibEcs
        LibRender2d
        LibImagery
        LibMap
        LibOrchestration
    PRIVATE
        LibQml
)
