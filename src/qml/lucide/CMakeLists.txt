set(LUCIDE_QML_SOURCE_QML_DIR "${lucide-qml_SOURCE_DIR}/src/qml")

set_source_files_properties(
    "${LUCIDE_QML_SOURCE_QML_DIR}/LucideIcon.qml"
    PROPERTIES
        QT_RESOURCE_ALIAS LucideIcon.qml
)

set_source_files_properties(
    "${LUCIDE_QML_SOURCE_QML_DIR}/lucide_icons.js"
    PROPERTIES
        QT_RESOURCE_ALIAS lucide_icons.js
)

set_source_files_properties(
    "${LUCIDE_QML_SOURCE_QML_DIR}/LICENSE-lucide.txt"
    PROPERTIES
        QT_RESOURCE_ALIAS LICENSE-lucide.txt
)

qt_add_qml_module(
    LucideQml
    URI Lucide
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Lucide"
    QML_FILES
        "${LUCIDE_QML_SOURCE_QML_DIR}/LucideIcon.qml"
    RESOURCES
        "${LUCIDE_QML_SOURCE_QML_DIR}/lucide_icons.js"
        "${LUCIDE_QML_SOURCE_QML_DIR}/LICENSE-lucide.txt"
    NO_LINT
)

add_library(LucideQml::LucideQml ALIAS LucideQml)

target_link_libraries(
    LucideQml
    PUBLIC
        Qt6::Quick
        Qt6::QuickShapes
)
