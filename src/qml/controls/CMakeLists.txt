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
        AboutDialog.qml
        BottomBar.qml
        ConfirmDialog.qml
        DrawerSide.qml
        DropdownMenu.qml
        HorizontalDrawer.qml
        IconButton.qml
        MenuButton.qml
        TextButton.qml
        TextInput.qml
        TopBar.qml
        VerticalDrawer.qml
    NO_LINT
)
