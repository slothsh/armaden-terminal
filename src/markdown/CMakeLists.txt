file(GLOB_RECURSE MARKDOWN_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibMarkdown
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${MARKDOWN_MODULE_SRC_FILES}
)

set_target_properties(
    LibMarkdown
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibMarkdown
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibMarkdown
    PUBLIC
        md4c
        LibRegex
)
