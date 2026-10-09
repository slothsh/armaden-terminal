find_package(Qt6 6.11 REQUIRED COMPONENTS Gui)

file(GLOB_RECURSE IMAGERY_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibImagery
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${IMAGERY_MODULE_SRC_FILES}
)

set_target_properties(
    LibImagery
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibImagery
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibImagery
    PUBLIC
        LibConfig
        LibPlatform
        LibCore
        Qt6::Gui
)
