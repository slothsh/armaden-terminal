file(GLOB_RECURSE PERSISTENCE_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibPersistence
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${PERSISTENCE_MODULE_SRC_FILES}
)

set_target_properties(
    LibPersistence
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibPersistence
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibPersistence
    PUBLIC
        LibAgent
        LibCore
)
