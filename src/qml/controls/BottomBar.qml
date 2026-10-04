import QtQuick
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    signal historyToggled()

    property bool historyOpen: false

    implicitHeight: 32
    color: Theme.color.medium

    IconButton {
        glyph: "\uD83D\uDCAC\uFE0E"
        glyphColor: control.historyOpen ? Theme.color.accent : Theme.color.textMuted
        anchors.left: parent.left
        anchors.leftMargin: Theme.metric.spacing
        anchors.verticalCenter: parent.verticalCenter
        onClicked: control.historyToggled()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.color.borderStrong
    }
}
