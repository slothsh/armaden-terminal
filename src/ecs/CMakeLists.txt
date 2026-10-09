find_package(Qt6 6.11 REQUIRED COMPONENTS Gui CanvasPainter)

file(GLOB_RECURSE ECS_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibEcs
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${ECS_MODULE_SRC_FILES}
)

set_target_properties(
    LibEcs
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibEcs
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibEcs
    PUBLIC
        EnTT::EnTT
        Qt6::Gui
        Qt6::CanvasPainter
)
