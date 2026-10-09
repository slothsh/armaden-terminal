import QtQuick
import Lib.Theme
import Lucide

Rectangle {
    id: control

    required property string iconName
    property real iconSize: 16
    property color iconColor: Theme.color.text
    property bool active: false
    property bool interactive: true
    property alias hovered: mouseArea.containsMouse

    signal clicked()

    implicitWidth: 24
    implicitHeight: 24
    radius: Theme.metric.radius
    opacity: control.interactive ? 1.0 : 0.4
    color: control.interactive && control.hovered
        ? Theme.color.hover
        : (control.interactive && control.active ? Theme.color.active : "transparent")

    LucideIcon {
        anchors.centerIn: parent
        name: control.iconName
        size: control.iconSize
        color: control.iconColor
        strokeWidth: 2
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: control.interactive ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        onClicked: {
            if (control.interactive) {
                control.clicked()
            }
        }
    }
}
