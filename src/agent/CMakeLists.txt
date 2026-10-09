file(GLOB_RECURSE AGENT_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibAgent
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${AGENT_MODULE_SRC_FILES}
)

set_target_properties(
    LibAgent
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibAgent
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibAgent
    PUBLIC
        LibCore
        LibMagicEnum
        lgc::langgraph
)
