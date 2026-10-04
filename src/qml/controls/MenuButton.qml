import QtQuick
import Lib.Theme

Rectangle {
    id: control

    required property string label
    property alias hovered: mouseArea.containsMouse

    signal clicked()

    implicitHeight: 26
    implicitWidth: 160
    radius: 2
    color: control.hovered ? Theme.color.hover : "transparent"

    Text {
        anchors.verticalCenter: parent.verticalCenter
        x: 8
        text: control.label
        color: Theme.color.text
        font.pixelSize: 12
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }
}
