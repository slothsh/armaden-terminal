import QtQuick
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    signal menuRequested()
    signal closeRequested()

    property bool menuActive: false
    readonly property real menuAnchorBottom: menuButton.y + menuButton.height

    implicitHeight: Theme.metric.topBarHeight
    color: Theme.color.medium

    IconButton {
        id: menuButton
        glyph: "\u2630"
        active: control.menuActive
        anchors.left: parent.left
        anchors.leftMargin: Theme.metric.spacing
        anchors.verticalCenter: parent.verticalCenter
        onClicked: control.menuRequested()
    }

    IconButton {
        id: closeButton
        glyph: "\u2715"
        anchors.right: parent.right
        anchors.rightMargin: Theme.metric.spacing
        anchors.verticalCenter: parent.verticalCenter
        onClicked: control.closeRequested()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.color.borderStrong
    }
}
