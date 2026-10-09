file(GLOB_RECURSE EVENTS_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibEvents
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${EVENTS_MODULE_SRC_FILES}
)

set_target_properties(
    LibEvents
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibEvents
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibEvents
    PUBLIC
        LibPlatform
)
