import QtQuick
import QtQuick as Quick
import Lib.Controls
import Lib.Interaction
import Lib.Theme

Rectangle {
    id: sessionRow

    required property int index
    required property string sessionId
    required property string title
    required property bool running

    property var interactionController: null
    property var contextMenu: null
    property Item overlay: null
    property string renamingSession: ""

    signal renameCancelled()
    signal renameAccepted(string sessionId, string title)

    readonly property bool renaming: sessionRow.renamingSession === sessionRow.sessionId

    readonly property bool selected: sessionRow.interactionController
        && sessionRow.interactionController.selectionRevision >= 0
        && sessionRow.interactionController.isSelected(sessionRow.InteractionScope.targetId)

    InteractionScope.domain: "app.sessionList"
    InteractionScope.role: "session-row"
    InteractionScope.semanticKey: sessionRow.sessionId
    InteractionScope.label: sessionRow.title
    InteractionScope.group: "session-list"
    InteractionScope.order: sessionRow.index * 2
    InteractionScope.focusable: !sessionRow.renaming
    InteractionScope.selectable: true
    InteractionScope.activatable: true
    InteractionScope.contextMenuable: true
    InteractionScope.navigable: true
    InteractionScope.selectionMode: "multiple"

    width: ListView.view.width
    height: 28

    function selectWithModifiers(modifiers) {
        const intent = sessionRow.interactionController.selectionIntent(
            (modifiers & Qt.ShiftModifier) !== 0,
            (modifiers & Qt.ControlModifier) !== 0
        )
        sessionRow.interactionController.executeCommand({
            kind: "select",
            domain: "app.sessionList",
            targets: [sessionRow.InteractionScope.targetId],
            intent: intent,
        })
    }

    function activate() {
        sessionRow.selectWithModifiers(0)
    }

    color: sessionRow.sessionId === chat.activeSession || sessionRow.selected
        ? Theme.color.active
        : ((sessionRow.interactionController.hoveredTarget === sessionRow.InteractionScope.targetId
            || (sessionRow.contextMenu && sessionRow.contextMenu.opened && sessionRow.contextMenu.actionTarget === sessionRow.InteractionScope.targetId)
            || sessionRow.activeFocus) ? Theme.color.hover : "transparent")
    border.color: sessionRow.activeFocus ? Theme.color.accent : "transparent"
    border.width: sessionRow.activeFocus ? 1 : 0

    MouseArea {
        anchors.fill: parent
        enabled: !sessionRow.renaming
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: mouse => {
            if (mouse.button === Qt.RightButton) {
                if (!sessionRow.overlay) {
                    return
                }

                const point = sessionRow.mapToItem(sessionRow.overlay, mouse.x, mouse.y)
                sessionRow.interactionController.executeCommand({
                    kind: "openContextMenu",
                    target: sessionRow.InteractionScope.targetId,
                    x: point.x,
                    y: point.y,
                })
                return
            }
            sessionRow.selectWithModifiers(mouse.modifiers)
        }
    }

    Text {
        visible: !sessionRow.renaming
        anchors.left: parent.left
        anchors.right: sessionSpinner.left
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: sessionRow.title
        color: Theme.color.text
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    Quick.TextInput {
        id: sessionRename
        InteractionScope.domain: "app.sessionList"
        InteractionScope.role: "session-rename"
        InteractionScope.semanticKey: sessionRow.sessionId
        InteractionScope.order: sessionRow.index
        InteractionScope.group: ""
        InteractionScope.focusable: true
        InteractionScope.editable: true
        InteractionScope.navigable: true
        visible: sessionRow.renaming
        anchors.left: parent.left
        anchors.right: sessionMenuButton.left
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: ""
        color: Theme.color.text
        font.pixelSize: 12
        clip: true
        selectByMouse: true
        selectionColor: Theme.color.accent
        selectedTextColor: Theme.color.text
        cursorDelegate: Rectangle {
            width: 2
            color: Theme.color.accent
        }

        function cancelEditing() {
            sessionRow.renameCancelled()
        }

        onAccepted: sessionRow.renameAccepted(sessionRow.sessionId, text)

        onActiveFocusChanged: {
            if (!activeFocus && sessionRow.renaming) {
                sessionRow.renameAccepted(sessionRow.sessionId, text)
            }
        }

        onVisibleChanged: {
            if (visible) {
                text = sessionRow.title
                forceActiveFocus()
                selectAll()
            }
        }
    }

    Spinner {
        id: sessionSpinner
        visible: sessionRow.running
        width: 12
        height: 12
        anchors.right: sessionMenuButton.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
    }

    IconButton {
        id: sessionMenuButton
        iconName: "ellipsis"
        interactionController: sessionRow.interactionController
        accessibleLabel: qsTr("Session actions")
        interactionDomain: "app.sessionList"
        interactionGroup: "session-list"
        interactionOrder: sessionRow.index * 2 + 1
        active: sessionRow.contextMenu
            && sessionRow.contextMenu.opened
            && sessionRow.contextMenu.actionTarget === sessionRow.InteractionScope.targetId
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        onClicked: {
            const target = sessionRow.InteractionScope.targetId
            if (sessionRow.contextMenu && sessionRow.contextMenu.opened && sessionRow.contextMenu.actionTarget === target) {
                sessionRow.interactionController.executeCommand({kind: "closeContextMenu"})
            } else {
                if (!sessionRow.overlay) {
                    return
                }

                const point = sessionMenuButton.mapToItem(sessionRow.overlay, 0, sessionMenuButton.height)
                sessionRow.interactionController.executeCommand({
                    kind: "openContextMenu",
                    target: target,
                    x: point.x,
                    y: point.y,
                })
            }
        }
    }
}
