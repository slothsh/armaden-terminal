file(GLOB_RECURSE CORE_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibCore
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${CORE_MODULE_SRC_FILES}
)

set_target_properties(
    LibCore
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibCore
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibCore
    PUBLIC
        spdlog::spdlog
        LibMagicEnum
)
