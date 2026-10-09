file(GLOB_RECURSE PLATFORM_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibPlatform
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${PLATFORM_MODULE_SRC_FILES}
)

set_target_properties(
    LibPlatform
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibPlatform
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibPlatform
    PUBLIC
        LibCore
        STDEXEC::stdexec
)
