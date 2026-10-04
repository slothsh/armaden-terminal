# Controls Module
set_source_files_properties(
    DrawerSide.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(
    controls
    URI Lib.Controls
    QML_FILES
        DrawerSide.qml
        HorizontalDrawer.qml
        VerticalDrawer.qml
    NO_LINT
)
