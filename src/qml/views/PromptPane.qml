import QtQuick
import QtQuick.Effects
import Lib.Controls
import Lib.Interaction
import Lib.Theme

Item {
    id: control

    property var chatController: null
    property var interactionController: null
    property real contentHeight: 0
    property Item overlay: null
    property bool chatActive: true

    readonly property alias drawerSize: inputDrawer.size
    readonly property var promptTargetId: promptInput.InteractionScope.targetId

    signal contextMenuRequested(real x, real y)

    height: inputDrawer.height

    function toggle() {
        inputDrawer.toggle()
    }

    function currentText() {
        return promptInput.text
    }

    function clearInput() {
        promptInput.text = ""
    }

    function saveDraft() {
        control.chatController.saveDraft(promptInput.text)
    }

    RectangularShadow {
        anchors.fill: inputDrawer
        visible: control.chatActive && inputDrawer.height > 0
        radius: inputDrawer.radius
        color: "#20000000"
        blur: 10
        spread: 2
        offset: Qt.point(0, -2)
        z: 6
    }

    VerticalDrawer {
        id: inputDrawer
        interactionDomain: "app.promptPane"
        side: DrawerSide.Bottom
        size: 120
        sizeMin: 80
        sizeMax: Math.max(80, Math.round(control.contentHeight / 2))
        z: 7
        color: Theme.color.medium

        Rectangle {
            id: inputGutter
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 30
            color: Theme.color.mediumBright

            IconButton {
                id: sendButton
                iconName: "send"
                anchors.left: parent.left
                anchors.leftMargin: Theme.metric.spacing
                anchors.verticalCenter: parent.verticalCenter
                actionId: "prompt.send"
                interactionDomain: "app.prompt"
                interactionGroup: "prompt"
                interactionOrder: 1
                interactionController: control.interactionController
                actionTarget: control.promptTargetId
                onClicked: control.interactionController.invokeAction(actionId, actionTarget)
            }

            IconButton {
                id: stopButton
                iconName: "square"
                actionId: "prompt.cancel"
                interactionDomain: "app.prompt"
                interactionGroup: "prompt"
                interactionOrder: 2
                interactionController: control.interactionController
                actionTarget: control.promptTargetId
                anchors.left: sendButton.right
                anchors.leftMargin: Theme.metric.spacing
                anchors.verticalCenter: parent.verticalCenter
                visible: control.chatController.running
                onClicked: control.interactionController.invokeAction(actionId, actionTarget)
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.color.borderStrong
            }
        }

        TextInput {
            id: promptInput
            interactionDomain: "app.prompt"
            interactionGroup: "prompt"
            interactionOrder: 0
            anchors.top: inputGutter.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            placeholderText: qsTr("Message the agent...")
            readOnly: control.chatController.running
            onTextChanged: {
                draftSaveTimer.restart()
                control.interactionController.setBusinessValue("prompt.text", text.trim())
            }
            Component.onCompleted: text = control.chatController.draft
            onAccepted: control.interactionController.invokeAction("prompt.send", control.promptTargetId)
            onContextMenuRequested: (x, y) => {
                if (!control.overlay) {
                    return
                }

                const point = promptInput.mapToItem(control.overlay, x, y)
                control.contextMenuRequested(point.x, point.y)
            }
        }
    }

    Timer {
        id: draftSaveTimer
        interval: 300
        onTriggered: control.chatController.saveDraft(promptInput.text)
    }

    Connections {
        target: control.chatController

        function onDraftChanged() {
            promptInput.text = control.chatController.draft
        }
    }
}
