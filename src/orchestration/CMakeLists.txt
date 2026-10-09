file(GLOB_RECURSE ORCHESTRATION_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibOrchestration
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${ORCHESTRATION_MODULE_SRC_FILES}
)

set_target_properties(
    LibOrchestration
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibOrchestration
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibOrchestration
    PUBLIC
        LibCore
        LibFoundation
)
