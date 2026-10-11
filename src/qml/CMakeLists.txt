add_library(LibQml)
set_target_properties(LibQml PROPERTIES LINKER_LANGUAGE CXX)

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
