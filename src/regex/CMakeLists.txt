file(GLOB_RECURSE REGEX_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibRegex
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${REGEX_MODULE_SRC_FILES}
)

set_target_properties(
    LibRegex
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibRegex
    PRIVATE
        "${ctre_SOURCE_DIR}/single-header"
        "${CMAKE_SOURCE_DIR}/src/include"
)
