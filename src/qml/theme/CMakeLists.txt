set_source_files_properties(
    Theme.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(
    theme
    URI Lib.Theme
    QML_FILES
        Theme.qml
    NO_LINT
)
