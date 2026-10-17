import QtQuick
import QtQuick.Window
import Lib.Interaction
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    InteractionScope.domain: "app.menuBar"
    InteractionScope.role: "pane"

    signal menuRequested()

    property bool menuActive: false
    property var interactionController: null
    property var actionTarget: 0
    property real contentX: 0
    readonly property real menuAnchorBottom: menuButton.y + menuButton.height

    implicitHeight: Theme.metric.menuBarHeight
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
        iconName: "menu"
        accessibleLabel: qsTr("Application menu")
        interactionDomain: "app.menuBar"
        interactionGroup: "menu-bar"
        interactionOrder: 0
        interactionController: control.interactionController
        active: control.menuActive
        anchors.left: parent.left
        anchors.leftMargin: Theme.metric.spacing
        anchors.verticalCenter: parent.verticalCenter
        onClicked: control.menuRequested()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.color.borderStrong
    }
}
