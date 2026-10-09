find_package(Qt6 6.11 REQUIRED COMPONENTS Gui CanvasPainter)

file(GLOB_RECURSE RENDER2D_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibRender2d
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${RENDER2D_MODULE_SRC_FILES}
)

set_target_properties(
    LibRender2d
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibRender2d
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibRender2d
    PUBLIC
        LibEcs
        Qt6::Gui
        Qt6::CanvasPainter
)
