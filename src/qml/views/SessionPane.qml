import QtQuick
import QtQuick.Effects
import Lib.Controls
import Lib.Interaction
import Lib.Theme

Item {
    id: control

    property var chatController: null
    property var interactionController: null
    property string renamingSession: ""
    property real topBarHeight: 0
    property real bottomBarHeight: 0
    property var contextMenu: null
    property Item overlay: null

    readonly property alias drawerWidth: leftDrawer.width

    signal createSessionRequested()
    signal renameCancelled()
    signal renameAccepted(string sessionId, string title)

    function toggleHistory() {
        leftDrawer.toggle()
    }

    RectangularShadow {
        anchors.fill: leftDrawer
        visible: leftDrawer.width > 0
        radius: leftDrawer.radius
        color: "#20000000"
        blur: 10
        spread: 2
        offset: Qt.point(2, 0)
    }

    HorizontalDrawer {
        id: leftDrawer
        interactionDomain: "app.sessionPane"
        size: 220
        sizeMin: 140
        sizeMax: control.width > 500 ? control.width / 2 : 250
        y: control.topBarHeight
        height: control.height - control.topBarHeight - control.bottomBarHeight
        z: 8

        color: Theme.color.menu

        Rectangle {
            id: historyGutter
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 30
            color: Theme.color.mediumBright
            clip: true

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Session History")
                color: Theme.color.text
                font.pixelSize: 12
            }

            IconButton {
                id: clearHistoryButton
                iconName: "ellipsis"
                accessibleLabel: qsTr("History actions")
                interactionController: control.interactionController
                interactionDomain: "app.sessionPane"
                interactionGroup: "session-toolbar"
                interactionOrder: 0
                active: historyMenu.opened
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                onClicked: historyMenu.toggle()
            }

            IconButton {
                id: addSessionButton
                iconName: "plus"
                accessibleLabel: qsTr("Create session")
                interactionController: control.interactionController
                interactionDomain: "app.sessionPane"
                interactionGroup: "session-toolbar"
                interactionOrder: 1
                anchors.right: clearHistoryButton.left
                anchors.verticalCenter: parent.verticalCenter
                onClicked: control.createSessionRequested()
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.color.borderStrong
            }
        }

        DropdownMenu {
            id: historyMenu
            interactionController: control.interactionController
            interactionDomain: "app.historyMenu"
            x: leftDrawer.width - width
            y: historyGutter.y + clearHistoryButton.y + clearHistoryButton.height + 2

        }

        ListView {
            id: sessionView
            anchors.top: historyGutter.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            clip: true
            model: control.chatController.sessions

            delegate: SessionRow {
                width: ListView.view.width
                interactionController: control.interactionController
                contextMenu: control.contextMenu
                overlay: control.overlay
                renamingSession: control.renamingSession
                onRenameCancelled: control.renameCancelled()
                onRenameAccepted: (sessionId, title) => control.renameAccepted(sessionId, title)
            }
        }
    }
}
