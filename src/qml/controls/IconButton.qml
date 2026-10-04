import QtQuick
import Lib.Theme

Rectangle {
    id: control

    required property string glyph
    property color glyphColor: Theme.color.text
    property bool active: false
    property alias hovered: mouseArea.containsMouse

    signal clicked()

    implicitWidth: 18
    implicitHeight: 18
    radius: Theme.metric.radius
    color: control.hovered ? Theme.color.hover : (control.active ? Theme.color.active : "transparent")

    Text {
        anchors.centerIn: parent
        text: control.glyph
        color: control.glyphColor
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
