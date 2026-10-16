file(GLOB_RECURSE CONFIG_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibConfig
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${CONFIG_MODULE_SRC_FILES}
)

set_target_properties(
    LibConfig
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibConfig
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibConfig
    PUBLIC
        LibAgent
        LibCore
)
