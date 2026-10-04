import QtQuick
import Lib.Theme

Rectangle {
    id: control

    required property string label
    property alias hovered: mouseArea.containsMouse

    signal clicked()

    implicitWidth: labelText.implicitWidth + 24
    implicitHeight: 30
    radius: Theme.metric.radius
    color: control.hovered ? Theme.color.hover : "transparent"

    Text {
        id: labelText
        anchors.centerIn: parent
        text: control.label
        color: Theme.color.text
        font.pixelSize: 13
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }
}
