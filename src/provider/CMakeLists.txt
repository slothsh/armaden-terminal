file(GLOB_RECURSE PROVIDER_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibProvider
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${PROVIDER_MODULE_SRC_FILES}
)

set_target_properties(
    LibProvider
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibProvider
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibProvider
    PUBLIC
        LibAgent
        LibConfig
        LibCore
)
