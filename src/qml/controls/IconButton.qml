import QtQuick
import Lib.Theme

Rectangle {
    id: control

    required property string glyph
    property alias hovered: mouseArea.containsMouse

    signal clicked()

    implicitWidth: 18
    implicitHeight: 18
    radius: Theme.metric.radius
    color: control.hovered ? Theme.color.hover : "transparent"

    Text {
        anchors.centerIn: parent
        text: control.glyph
        color: Theme.color.text
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }
}
