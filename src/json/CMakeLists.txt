file(GLOB_RECURSE JSON_MODULE_SRC_FILES CONFIGURE_DEPENDS *.cppm)

add_library_target(
    LibJson
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        ${JSON_MODULE_SRC_FILES}
)

set_target_properties(
    LibJson
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibJson
    PRIVATE
        "${langgraph_cpp_SOURCE_DIR}/third_party/nlohmann_json/include"
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibJson
    PUBLIC
        spdlog::spdlog
)
