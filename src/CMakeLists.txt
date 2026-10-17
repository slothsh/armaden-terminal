# Project Qt source/header files
file(GLOB_RECURSE QOBJECTS_MODULE_SRC_FILES CONFIGURE_DEPENDS qobjects/*.cpp)
file(GLOB_RECURSE QOBJECTS_MODULE_HEADER_FILES CONFIGURE_DEPENDS qobjects/*.hpp)

list(FILTER QOBJECTS_MODULE_SRC_FILES EXCLUDE REGEX "interaction_(scope|hub)\\.cpp$")
list(FILTER QOBJECTS_MODULE_HEADER_FILES EXCLUDE REGEX "interaction_(scope|hub)\\.hpp$")
list(FILTER QOBJECTS_MODULE_SRC_FILES EXCLUDE REGEX "virtual_list_(view|layout)\\.cpp$")
list(FILTER QOBJECTS_MODULE_HEADER_FILES EXCLUDE REGEX "virtual_list_(view|layout)\\.hpp$")

# Project module source files
file(GLOB_RECURSE LOGGING_MODULE_SRC_FILES CONFIGURE_DEPENDS logging/*.cppm)

find_package(EnTT REQUIRED)
find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickShapes CanvasPainter Network)
qt_standard_project_setup(REQUIRES 6.11)

add_subdirectory(third_party/)
add_subdirectory(json/)
add_subdirectory(core/)
add_subdirectory(interaction/)
add_subdirectory(markdown/)
add_subdirectory(regex/)

qt_add_qml_module(
    interactionui
    URI Lib.Interaction
    VERSION 1.0
    SOURCES
        qobjects/interaction_hub.hpp
        qobjects/interaction_hub.cpp
        qobjects/interaction_scope.hpp
        qobjects/interaction_scope.cpp
        qobjects/virtual_list_view.hpp
        qobjects/virtual_list_view.cpp
        qobjects/virtual_list_layout.hpp
        qobjects/virtual_list_layout.cpp
    NO_LINT
)

target_include_directories(
    interactionui
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        "${CMAKE_CURRENT_SOURCE_DIR}/qobjects/"
)

target_link_libraries(
    interactionui
    PUBLIC
        Qt6::Qml
        Qt6::Quick
)

add_subdirectory(qml/)
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

set(MTS_QML_MODULE_FILES
    qml/Main.qml
    qml/AppWindow.qml
    qml/AppEntry.qml
    qml/views/ConversationView.qml
    qml/views/ConversationBlock.qml
    qml/views/MarkdownView.qml
    qml/views/MarkdownNode.qml
    qml/views/MarkdownNodeDelegate.qml
    qml/views/MarkdownInlineContent.qml
    qml/views/MarkdownImage.qml
    qml/views/CodeBlock.qml
    qml/views/PromptPane.qml
    qml/views/SessionPane.qml
    qml/views/SessionRow.qml
    qml/views/QuitController.qml
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND MTS_QML_MODULE_FILES qml/DevHost.qml)
endif()

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
        ${MTS_QML_MODULE_FILES}
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
        Qt6::Network
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
        LibInteraction
        LibRender2d
        LibImagery
        LibMap
        LibOrchestration
        LibMarkdown
        LibRegex
    PRIVATE
        LibQml
        interactionui
)

configure_module_file(app/qml_hot_reload.cpp mts IS_SOURCE_FILE)

target_sources(
    mts
    PRIVATE
        "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/app/qml_hot_reload.cpp>"
)

target_compile_definitions(
    mts
    PRIVATE
        MTS_QML_HOT_RELOAD=$<CONFIG:Debug>
        "$<$<CONFIG:Debug>:MTS_QML_SOURCE_DIR_DEFAULT=\"${CMAKE_SOURCE_DIR}/src/qml\">"
)
