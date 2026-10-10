file(GLOB_RECURSE INTERACTION_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibInteraction
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${INTERACTION_MODULE_SRC_FILES}
)

set_target_properties(
    LibInteraction
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibInteraction
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibInteraction
    PUBLIC
        LibCore
)
