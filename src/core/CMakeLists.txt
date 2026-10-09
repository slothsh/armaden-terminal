file(GLOB_RECURSE CORE_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)
file(GLOB_RECURSE CORE_SOURCE_FILES CONFIGURE_DEPENDS *.cpp)

add_library_target(
    LibCore
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    SOURCE_FILES
        ${CORE_SOURCE_FILES}
    MODULE_FILES
        ${CORE_MODULE_SRC_FILES}
)

target_include_directories(
    LibCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

set_target_properties(
    LibCore
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_link_libraries(
    LibCore
    PUBLIC
        spdlog::spdlog
        STDEXEC::stdexec
        EnTT::EnTT
        LibFoundation
        lgc::langgraph
)
