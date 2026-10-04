import QtQuick
import QtQuick.Window
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    signal menuRequested()
    signal closeRequested()
    signal expandRequested()

    property bool menuActive: false
    property bool expandVisible: true
    property bool expandActive: false
    property bool expandEnabled: true
    property real contentX: 0
    property real contentInset: 0
    readonly property real menuAnchorBottom: menuButton.y + menuButton.height

    implicitHeight: Theme.metric.topBarHeight
    color: Theme.color.medium

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: {
            if (Window.window) {
                Window.window.startSystemMove()
            }
        }
    }

    IconButton {
        id: menuButton
        glyph: "\u2630"
        active: control.menuActive
        anchors.left: parent.left
        anchors.leftMargin: Theme.metric.spacing
        anchors.verticalCenter: parent.verticalCenter
        onClicked: control.menuRequested()
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        x: control.contentX - 1
        width: 1
        color: Theme.color.borderStrong
        visible: control.expandVisible && control.contentX > 0
    }

    IconButton {
        id: expandButton
        glyph: "\u2922"
        x: Math.max(control.contentX + Math.max(control.contentInset, Theme.metric.spacing), menuButton.x + menuButton.width + Theme.metric.spacing)
        anchors.verticalCenter: parent.verticalCenter
        visible: control.expandVisible
        interactive: control.expandEnabled
        glyphColor: control.expandActive ? Theme.color.accent : Theme.color.textMuted
        onClicked: control.expandRequested()
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
