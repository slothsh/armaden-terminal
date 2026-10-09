add_library(LibQml)

target_link_libraries(
    LibQml
    PUBLIC
        Qt::Quick
        controls
        theme
        LucideQml
)

add_subdirectory(controls/)
add_subdirectory(lucide/)
add_subdirectory(theme/)
