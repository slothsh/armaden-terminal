import QtQuick
import QtQuick.Controls
import Lib.Theme

Popup {
    id: control

    width: 160
    padding: 4
    implicitHeight: contentColumn.implicitHeight + padding * 2

    background: Rectangle {
        color: Theme.color.menu
        border.color: Theme.color.border
        border.width: 1
        radius: Theme.metric.radius
    }

    contentItem: Column {
        id: contentColumn
        spacing: 0
    }
}
