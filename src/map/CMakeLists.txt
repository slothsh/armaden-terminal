find_package(Qt6 6.11 REQUIRED COMPONENTS Gui)

add_library_target(
    LibMap
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        map.cppm
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
        LibCore
        LibEcs
        LibImagery
        Qt6::Gui
)
