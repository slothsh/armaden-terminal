add_library(LibQml)

target_link_libraries(
    LibQml
    PUBLIC
        Qt::Quick
        controls
        theme
)

add_subdirectory(controls/)
add_subdirectory(theme/)
