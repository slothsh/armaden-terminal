import QtQuick
import Lib.Interaction
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    InteractionScope.domain: "app.bottomBar"
    InteractionScope.role: "pane"

    signal historyRequested()
    signal inputDrawerRequested()
    signal mapToggleRequested()

    property var interactionController: null
    property var actionTarget: 0
    property real contentX: 0
    property bool historyActive: false
    property bool inputActive: false
    property bool mapActive: false

    implicitHeight: 32
    color: Theme.color.medium

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.color.borderStrong
    }

    IconButton {
        id: historyButton
        iconName: "history"
        interactionController: control.interactionController
        interactionDomain: "app.bottomBar"
        interactionGroup: "bottom-bar"
        interactionOrder: 0
        accessibleLabel: qsTr("Session history")
        iconColor: control.historyActive ? Theme.color.accent : Theme.color.textMuted
        x: Theme.metric.spacing
        anchors.verticalCenter: parent.verticalCenter
        onClicked: control.historyRequested()
    }

    IconButton {
        id: inputDrawerButton
        iconName: "keyboard"
        interactionController: control.interactionController
        accessibleLabel: qsTr("Show message input")
        interactionDomain: "app.bottomBar"
        interactionGroup: "bottom-bar"
        interactionOrder: 1
        iconColor: control.inputActive ? Theme.color.accent : Theme.color.textMuted
        x: Math.max(control.contentX + Theme.metric.spacing, historyButton.x + historyButton.width + Theme.metric.spacing)
        anchors.verticalCenter: parent.verticalCenter
        onClicked: control.inputDrawerRequested()
    }

    IconButton {
        id: mapPaneButton
        iconName: "map"
        interactionController: control.interactionController
        actionId: "map.toggle"
        interactionDomain: "app.bottomBar"
        interactionGroup: "bottom-bar"
        interactionOrder: 2
        actionTarget: control.actionTarget
        iconColor: control.mapActive ? Theme.color.accent : Theme.color.textMuted
        x: Math.max(control.contentX + Theme.metric.spacing, inputDrawerButton.x + inputDrawerButton.width + Theme.metric.spacing)
        anchors.verticalCenter: parent.verticalCenter
        onClicked: control.mapToggleRequested()
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: control.contentX - 1
        width: 1
        color: Theme.color.borderStrong
        visible: control.contentX > 0
    }
}
