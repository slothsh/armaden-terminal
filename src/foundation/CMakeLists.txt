add_library_target(
    LibFoundation
    STATIC
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}"
    MODULE_FILES
        foundation.cppm
)

set_target_properties(
    LibFoundation
    PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
)

target_include_directories(
    LibFoundation
    PRIVATE
        "${CMAKE_SOURCE_DIR}/src/include"
)

target_link_libraries(
    LibFoundation
    PUBLIC
        spdlog::spdlog
        LibMagicEnum
)

add_test_target(foundation_test foundation_test.cpp)
target_link_libraries(foundation_test PRIVATE LibFoundation)
