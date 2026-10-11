import QtQuick
import QtQuick.Window
import Lib.Interaction
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    InteractionScope.domain: "app.topBar"
    InteractionScope.role: "pane"

    signal menuRequested()
    signal closeRequested()

    property bool menuActive: false
    property string closeActionId: ""
    property var interactionController: null
    property var actionTarget: 0
    property real contentX: 0
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
        iconName: "menu"
        accessibleLabel: qsTr("Application menu")
        interactionDomain: "app.topBar"
        interactionGroup: "top-bar"
        interactionOrder: 0
        interactionController: control.interactionController
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
        visible: control.contentX > 0
    }

    IconButton {
        id: closeButton
        iconName: "x"
        accessibleLabel: qsTr("Close application")
        interactionDomain: "app.topBar"
        interactionGroup: "top-bar"
        interactionOrder: 1
        iconSize: 20
        actionId: control.closeActionId
        interactionController: control.interactionController
        actionTarget: control.actionTarget
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
