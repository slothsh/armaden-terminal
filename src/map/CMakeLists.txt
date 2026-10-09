find_package(Qt6 6.11 REQUIRED COMPONENTS Gui)

file(GLOB_RECURSE MAP_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibMap
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${MAP_MODULE_SRC_FILES}
)

set_target_properties(
    LibMap
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibMap
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibMap
    PUBLIC
        LibConfig
        LibPlatform
        LibCore
        LibJson
        LibEcs
        LibRender2d
        LibImagery
        Qt6::Gui
)
