import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Lib.Theme

Popup {
    id: control

    width: 160
    padding: 4
    implicitHeight: contentColumn.implicitHeight + padding * 2

    background: Item {
        RectangularShadow {
            anchors.fill: parent
            radius: Theme.metric.radius
            color: "#40000000"
            blur: 16
            spread: 0
            offset: Qt.point(0, 4)
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.color.menu
            border.color: Theme.color.border
            border.width: 1
            radius: Theme.metric.radius
        }
    }

    contentItem: Column {
        id: contentColumn
        spacing: 0
    }
}
