import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import Lib.Interaction
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    InteractionScope.domain: "app.menuBar"
    InteractionScope.role: "pane"

    signal closeRequested()
    signal maximizeRequested()
    signal minimizeRequested()

    property string closeActionId: ""
    property string maximizeActionId: ""
    property string minimizeActionId: ""
    property var interactionController: null
    property var actionTarget: 0

    implicitHeight: Theme.metric.menuBarHeight
    color: Theme.color.dark

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onPressed: {
            if (Window.window) {
                Window.window.startSystemMove()
            }
        }
    }

    // TODO: TabManager
    Rectangle {
        id: tab
        width: 200
        height: control.height - Theme.metric.spacing
        x: windowControls.mapToItem(control, windowControls.width, 0).x + Theme.metric.spacing*2
        y: Theme.metric.spacing
        topRightRadius: 6
        topLeftRadius: 6
        color: Theme.color.medium
        z: 1

        FlexboxLayout {
            anchors.fill: parent
            wrap: FlexboxLayout.Wrap
            direction: FlexboxLayout.Row
            justifyContent: FlexboxLayout.JustifySpaceBetween
            alignItems: FlexboxLayout.AlignCenter
            anchors.leftMargin: Theme.metric.spacing * 1.5
            anchors.rightMargin: Theme.metric.spacing * 1.5

            Text {
                text: "Session 1"
                font.pointSize: Theme.font.pointSize * 0.75
                color: Theme.color.text
            }

            IconButton {
                iconName: "x"
                accessibleLabel: qsTr("Close application")
                interactionDomain: "app.menuBar"
                interactionGroup: "menu-bar"
                interactionOrder: 0
                iconSize: 12
                iconColor: Theme.color.textMuted
                outerWidth: 14
                outerHeight: 14
                actionId: control.closeActionId
                interactionController: control.interactionController
                actionTarget: control.actionTarget
                onClicked: {
                    console.log("TODO: not implemented")
                }
            }
        }
    }

    FlexboxLayout {
        anchors.fill: parent
        wrap: FlexboxLayout.Wrap
        direction: FlexboxLayout.Row
        justifyContent: FlexboxLayout.JustifySpaceBetween
        alignItems: FlexboxLayout.AlignCenter
        anchors.rightMargin: Theme.metric.spacing
        anchors.leftMargin: Theme.metric.spacing

        FlexboxLayout {
            id: windowControls
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            wrap: FlexboxLayout.Wrap
            direction: FlexboxLayout.Row
            justifyContent: FlexboxLayout.JustifySpaceBetween
            alignItems: FlexboxLayout.AlignCenter
            columnGap: 4

            IconButton {
                id: closeButton
                iconName: "x"
                accessibleLabel: qsTr("Close application")
                interactionDomain: "app.menuBar"
                interactionGroup: "menu-bar"
                interactionOrder: 0
                iconSize: 20
                actionId: control.closeActionId
                interactionController: control.interactionController
                actionTarget: control.actionTarget
                onClicked: control.closeRequested()
            }

            IconButton {
                id: maximizeButton
                iconName: "square"
                accessibleLabel: qsTr("Maximize application")
                interactionDomain: "app.menuBar"
                interactionGroup: "menu-bar"
                interactionOrder: 1
                iconSize: 16
                actionId: control.maximizeActionId
                interactionController: control.interactionController
                actionTarget: control.actionTarget
                onClicked: control.maximizeRequested()
            }

            IconButton {
                id: minimizeButton
                iconName: "minus"
                accessibleLabel: qsTr("Minimize application")
                interactionDomain: "app.menuBar"
                interactionGroup: "menu-bar"
                interactionOrder: 2
                iconSize: 16
                actionId: control.minimizeActionId
                interactionController: control.interactionController
                actionTarget: control.actionTarget
                onClicked: control.minimizeRequested()
            }
        }

        Text {
            text: "Terminal 0.1.0"
            color: Theme.color.textDimmed
            font.pointSize: Theme.font.pointSize * 0.75
            font.weight: Font.Medium
        }
    }
}
