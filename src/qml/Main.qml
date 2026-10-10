import QtQuick
import QtQuick as Quick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Templates as T
import QtQuick.Effects
import Lib.Controls
import Lib.Interaction
import Lib.Theme

Window {
    id: root
    InteractionScope.domain: "app"
    InteractionScope.role: "window"
    InteractionScope.activatable: true
    visible: true
    color: Theme.color.dark
    width: 960
    height: 640
    title: qsTr("mindtosis")

    property var chat: null
    property var interaction: null
    property string pendingDelete: ""
    property string renamingSession: ""

    signal contextMenuRequested(real x, real y)

    onContextMenuRequested: (x, y) => {
        const available = root.interaction.actionsFor(root.interaction.hoveredTarget, "contextMenu")
        if (available.length === 0) {
            return
        }

        interactionContextMenu.actionTarget = root.interaction.hoveredTarget
        interactionContextMenu.openAt(x, y)
    }

    property bool mapPaneVisible: false
    property bool chatExpanded: false
    readonly property real chatPageWidth: 960
    readonly property real chatMarginMin: Theme.metric.spacing
    readonly property bool chatNarrow: content.width < chatPageWidth + chatMarginMin * 2
    readonly property bool chatFullWidth: chatExpanded || chatNarrow
    readonly property real chatColumnInset: chatFullWidth ? 0 : Math.round((content.width - chatPageWidth) / 2)
    readonly property real chatColumnWidth: chatFullWidth ? content.width : chatPageWidth

    property bool quitConfirmed: false

    function requestQuit() {
        quitDialog.open()
    }

    function performQuit() {
        quitConfirmed = true
        chat.cancelAll()
        if (!chat.hasInFlight()) {
            Qt.quit()
        } else {
            quitDrainTimer.restart()
            quitTimeoutTimer.restart()
        }
    }

    function commitRename(id, title) {
        if (root.renamingSession !== id) {
            return
        }

        root.renamingSession = ""
        const trimmed = title.trim()
        if (trimmed.length > 0) {
            chat.renameSession(id, trimmed)
        }
    }

    function sendPrompt() {
        conversationView.stickToBottom = true
        chat.sendMessage(promptInput.text)
        promptInput.text = ""
    }

    function handleInteractionAction(actionId, target, semanticKey) {
        switch (actionId) {
            case "session.rename": {
                root.renamingSession = semanticKey
                break
            }

            case "session.remove": {
                root.pendingDelete = semanticKey
                deleteDialog.open()
                break
            }

            case "history.clear": {
                clearDialog.open()
                break
            }

            case "app.about": {
                aboutDialog.open()
                break
            }

            case "app.quit": {
                root.requestQuit()
                break
            }

            case "prompt.send": {
                root.sendPrompt()
                break
            }

            case "prompt.cancel": {
                chat.cancel()
                break
            }

            case "map.fit": {
                mapCanvas.fitToMap()
                break
            }

            case "map.toggle": {
                root.mapPaneVisible = !root.mapPaneVisible
                break
            }
        }
    }

    Component.onCompleted: {
        interaction.setBusinessValue("chat.running", chat.running ? "true" : "false")
        interaction.setBusinessValue("prompt.text", promptInput.text.trim())
        interaction.setBusinessValue("map.ready", chat.mapController.ready ? "true" : "false")
    }

    onClosing: close => {
        if (!quitConfirmed) {
            close.accepted = false
            requestQuit()
        }
    }

    TopBar {
        id: topBar
        anchors.top: parent.top
        width: parent.width
        z: 9
        menuActive: appMenu.opened
        closeActionId: "app.quit"
        interactionController: root.interaction
        actionTarget: root.InteractionScope.targetId
        expandVisible: chat.conversation.count > 0
        expandActive: root.chatExpanded
        expandEnabled: !root.chatNarrow
        contentX: content.x
        contentInset: root.chatColumnInset
        onMenuRequested: appMenu.toggle()
        onCloseRequested: interaction.invokeAction("app.quit", root.InteractionScope.targetId)
        onExpandRequested: root.chatExpanded = !root.chatExpanded
    }

    BottomBar {
        id: bottomBar
        anchors.bottom: parent.bottom
        width: parent.width
        z: 9

        IconButton {
            id: historyButton
            iconName: "history"
            iconColor: leftDrawer.width > 0 ? Theme.color.accent : Theme.color.textMuted
            x: Theme.metric.spacing
            anchors.verticalCenter: parent.verticalCenter
            onClicked: leftDrawer.toggle()
        }

        IconButton {
            id: inputDrawerButton
            iconName: "keyboard"
            accessibleLabel: qsTr("Show message input")
            interactionDomain: "app.topBar"
            interactionGroup: "top-bar"
            interactionOrder: 3
            iconColor: inputDrawer.size > 0 ? Theme.color.accent : Theme.color.textMuted
            x: Math.max(content.x + Theme.metric.spacing, historyButton.x + historyButton.width + Theme.metric.spacing)
            anchors.verticalCenter: parent.verticalCenter
            onClicked: inputDrawer.toggle()
        }

        IconButton {
            id: mapPaneButton
            iconName: "map"
            actionId: "map.toggle"
            interactionDomain: "app.topBar"
            interactionGroup: "top-bar"
            interactionOrder: 4
            interactionController: root.interaction
            actionTarget: root.InteractionScope.targetId
            iconColor: root.mapPaneVisible ? Theme.color.accent : Theme.color.textMuted
            x: Math.max(content.x + Theme.metric.spacing, inputDrawerButton.x + inputDrawerButton.width + Theme.metric.spacing)
            anchors.verticalCenter: parent.verticalCenter
            onClicked: interaction.invokeAction(actionId, actionTarget)
        }

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            x: content.x - 1
            width: 1
            color: Theme.color.borderStrong
            visible: content.x > 0
        }
    }

    Rectangle {
        id: content
        InteractionScope.domain: "app.main"
        InteractionScope.role: "pane"
        x: leftDrawer.width
        y: topBar.height
        width: parent.width - leftDrawer.width
        height: parent.height - topBar.height - bottomBar.height
        color: Theme.color.dark
        T.ContextMenu.menu: null
        T.ContextMenu.onRequested: function(position) {
            const point = content.mapToItem(root.contentItem, position.x, position.y)
            root.contextMenuRequested(point.x, point.y)
        }

        MapCanvas {
            id: mapCanvas
            InteractionScope.domain: "app.map"
            InteractionScope.role: "surface"
            InteractionScope.hitTestable: true
            InteractionScope.contextMenuable: true
            InteractionScope.focusable: true
            InteractionScope.activatable: true
            InteractionScope.navigable: true
            InteractionScope.draggable: true
            InteractionScope.scrollable: true
            anchors.fill: parent
            visible: root.mapPaneVisible
            controller: chat.mapController
        }

        ScrollingArea {
            id: conversationView
            visible: !root.mapPaneVisible
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: inputDrawer.top
            stickToBottom: true
            contentCursorShape: Qt.IBeamCursor
            contentCursorStart: root.chatColumnInset
            contentCursorWidth: root.chatColumnWidth
            barTopMargin: Theme.metric.spacing
            barBottomMargin: Theme.metric.spacing

            Item {
                width: root.chatColumnWidth
                x: (conversationView.width - width) / 2
                height: threadColumn.height

                Column {
                    id: threadColumn
                    width: parent.width

                    Repeater {
                        model: chat.conversation

                        onItemAdded: {
                            if (conversationView.stickToBottom) {
                                conversationView.snapToEnd();
                            }
                        }

                        delegate: Rectangle {
                            width: threadColumn.width
                            height: blockText.contentHeight + 24
                            radius: 0
                            color: {
                                switch (model.kind) {
                                    case ConversationModel.UserText: return Theme.color.medium;
                                    case ConversationModel.Reasoning: return Theme.color.menu;
                                    case ConversationModel.ToolCall: return Theme.color.menu;
                                    case ConversationModel.ToolResult: return Theme.color.menu;
                                    case ConversationModel.Error: return "#3a1f1f";
                                    default: return Theme.color.menu;
                                }
                            }

                            Rectangle {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 1
                                visible: index > 0
                                color: Theme.color.borderStrong
                            }

                            TextEdit {
                                id: blockText
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 12
                                height: contentHeight
                                readOnly: true
                                selectByMouse: true
                                textFormat: TextEdit.PlainText
                                wrapMode: TextEdit.Wrap
                                text: model.kind === ConversationModel.ToolResult && model.payload.length > 0
                                    ? model.text + " " + model.payload
                                    : (model.kind === ConversationModel.ToolCall ? model.text + " " + model.payload : model.text)
                                color: model.kind === ConversationModel.Reasoning ? Theme.color.textMuted : Theme.color.text
                                font.pixelSize: 12
                                font.italic: model.kind === ConversationModel.Reasoning
                                font.family: (model.kind === ConversationModel.ToolCall || model.kind === ConversationModel.ToolResult)
                                    ? "monospace" : ""
                                selectionColor: Theme.color.accent
                                selectedTextColor: Theme.color.text
                            }
                        }
                    }
                }
            }
        }

        RectangularShadow {
            anchors.fill: inputDrawer
            visible: inputDrawer.height > 0
            radius: inputDrawer.radius
            color: "#20000000"
            blur: 10
            spread: 2
            offset: Qt.point(0, -2)
            z: 6
        }

        VerticalDrawer {
            id: inputDrawer
            InteractionScope.domain: "app.promptPane"
            InteractionScope.role: "pane"
            visible: !root.mapPaneVisible
            side: DrawerSide.Bottom
            size: 120
            sizeMin: 80
            sizeMax: Math.max(80, Math.round(content.height / 2))
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
                    interactionController: root.interaction
                    actionTarget: promptInput.InteractionScope.targetId
                    onClicked: interaction.invokeAction(actionId, actionTarget)
                }

                IconButton {
                    id: stopButton
                    iconName: "square"
                    actionId: "prompt.cancel"
                    interactionDomain: "app.prompt"
                    interactionGroup: "prompt"
                    interactionOrder: 2
                    interactionController: root.interaction
                    actionTarget: promptInput.InteractionScope.targetId
                    anchors.left: sendButton.right
                    anchors.leftMargin: Theme.metric.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    visible: chat.running
                    onClicked: interaction.invokeAction(actionId, actionTarget)
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
                readOnly: chat.running
                onTextChanged: {
                    draftSaveTimer.restart()
                    interaction.setBusinessValue("prompt.text", text.trim())
                }
                Component.onCompleted: text = chat.draft
                onAccepted: interaction.invokeAction("prompt.send", InteractionScope.targetId)
                onContextMenuRequested: (x, y) => {
                    const point = promptInput.mapToItem(root.contentItem, x, y)
                    root.contextMenuRequested(point.x, point.y)
                }
            }
        }

        Timer {
            id: draftSaveTimer
            interval: 300
            onTriggered: chat.saveDraft(promptInput.text)
        }

        Connections {
            target: chat

            function onDraftChanged() {
                promptInput.text = chat.draft
            }
        }
    }

    Rectangle {
        x: 0
        y: 0
        width: root.width
        height: root.height
        color: "transparent"

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
            InteractionScope.domain: "app.sessionPane"
            InteractionScope.role: "pane"
            size: 220
            sizeMin: 140
            sizeMax: root.width > 500 ? root.width / 2 : 250
            y: topBar.height
            height: root.height - topBar.height - bottomBar.height
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
                    active: historyMenu.opened
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: historyMenu.toggle()
                }

                IconButton {
                    id: addSessionButton
                    iconName: "plus"
                    anchors.right: clearHistoryButton.left
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: {
                        conversationView.stickToBottom = true
                        chat.saveDraft(promptInput.text)
                        chat.createSession()
                    }
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
                interactionController: root.interaction
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
                model: chat.sessions

                delegate: Rectangle {
                    id: sessionRow
                    width: ListView.view.width
                    height: 28
                    InteractionScope.domain: "app.sessionList"
                    InteractionScope.role: "session-row"
                    InteractionScope.semanticKey: model.sessionId
                    InteractionScope.label: model.title
                    InteractionScope.group: "session-list"
                    InteractionScope.order: index * 2
                    InteractionScope.focusable: !sessionRow.renaming
                    InteractionScope.selectable: true
                    InteractionScope.activatable: true
                    InteractionScope.contextMenuable: true
                    InteractionScope.navigable: true
                    InteractionScope.selectionMode: "multiple"

                    readonly property bool renaming: root.renamingSession === model.sessionId

                    function activate() {
                        conversationView.stickToBottom = true
                        chat.saveDraft(promptInput.text)
                        chat.selectSession(model.sessionId)
                    }

                    color: model.sessionId === chat.activeSession
                        ? Theme.color.active
                        : ((rowHover.hovered || sessionMenu.opened || sessionRow.activeFocus) ? Theme.color.hover : "transparent")
                    border.color: sessionRow.activeFocus ? Theme.color.accent : "transparent"
                    border.width: sessionRow.activeFocus ? 1 : 0

                    HoverHandler {
                        id: rowHover
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !sessionRow.renaming
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: mouse => {
                            if (mouse.button === Qt.RightButton) {
                                sessionMenu.actionTarget = sessionRow.InteractionScope.targetId
                                sessionMenu.openAt(mouse.x, mouse.y)
                                return
                            }
                            sessionRow.activate()
                        }
                    }

                    Text {
                        visible: !sessionRow.renaming
                        anchors.left: parent.left
                        anchors.right: model.running ? sessionSpinner.left : sessionMenuButton.left
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: model.title
                        color: Theme.color.text
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    Quick.TextInput {
                        id: sessionRename
                        InteractionScope.domain: "app.sessionList"
                        InteractionScope.role: "session-rename"
                        InteractionScope.semanticKey: model.sessionId
                        InteractionScope.order: index
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
                            root.renamingSession = ""
                        }

                        onAccepted: root.commitRename(model.sessionId, text)

                        onActiveFocusChanged: {
                            if (!activeFocus && sessionRow.renaming) {
                                root.commitRename(model.sessionId, text)
                            }
                        }

                        onVisibleChanged: {
                            if (visible) {
                                text = model.title
                                forceActiveFocus()
                                selectAll()
                            }
                        }
                    }

                    Spinner {
                        id: sessionSpinner
                        visible: model.running
                        width: 12
                        height: 12
                        anchors.right: sessionMenuButton.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    IconButton {
                        id: sessionMenuButton
                        iconName: "ellipsis"
                        accessibleLabel: qsTr("Session actions")
                        interactionDomain: "app.sessionList"
                        interactionGroup: "session-list"
                        interactionOrder: index * 2 + 1
                        active: sessionMenu.opened
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: {
                            sessionMenu.actionTarget = sessionRow.InteractionScope.targetId
                            sessionMenu.x = sessionRow.width - sessionMenu.width
                            sessionMenu.y = sessionRow.height + 2
                            sessionMenu.toggle()
                        }
                    }

                    ContextMenu {
                        id: sessionMenu
                        interactionController: root.interaction
                        interactionDomain: "app.sessionMenu"
                        layerKind: "contextMenu"
                    }
                }
            }
        }
    }

    DropdownMenu {
        id: appMenu
        interactionController: root.interaction
        interactionDomain: "app.appMenu"
        x: Theme.metric.spacing
        y: topBar.menuAnchorBottom + 2
    }

    ContextMenu {
        id: interactionContextMenu
        interactionController: root.interaction
        interactionDomain: "app.contextMenu"
    }

    Connections {
        target: root.interaction

        function onActionRequested(actionId, target, semanticKey) {
            root.handleInteractionAction(actionId, target, semanticKey)
        }

        function onContextMenuForTarget(target, x, y) {
            interactionContextMenu.actionTarget = target
            interactionContextMenu.openAt(x, y)
        }
    }

    Connections {
        target: chat

        function onRunningChanged() {
            root.interaction.setBusinessValue("chat.running", chat.running ? "true" : "false")
        }
    }

    Connections {
        target: chat.mapController

        function onReadyChanged() {
            root.interaction.setBusinessValue("map.ready", chat.mapController.ready ? "true" : "false")
        }
    }

    ConfirmDialog {
        id: deleteDialog
        interactionController: root.interaction
        message: qsTr("Are you sure you want to delete this session's history?")
        onAccepted: chat.deleteSession(root.pendingDelete)
    }

    ConfirmDialog {
        id: clearDialog
        interactionController: root.interaction
        message: qsTr("Are you sure you want to clear all conversation history?")
        onAccepted: chat.clearHistory()
    }

    AboutDialog {
        id: aboutDialog
        interactionController: root.interaction
        title: qsTr("mindtosis")
        version: qsTr("Version 0.0.0")
        description: chat.status.length > 0 ? chat.status : qsTr("An AI agent harness.")
    }

    Timer {
        id: quitDrainTimer
        interval: 100
        repeat: true
        onTriggered: {
            if (!chat.hasInFlight()) {
                stop()
                quitTimeoutTimer.stop()
                Qt.quit()
            }
        }
    }

    Timer {
        id: quitTimeoutTimer
        interval: 5000
        onTriggered: {
            quitDrainTimer.stop()
            Qt.quit()
        }
    }

    ConfirmDialog {
        id: quitDialog
        interactionController: root.interaction
        message: qsTr("Are you sure you want to exit? Any in-flight responses will be cancelled.")
        confirmText: qsTr("Quit")
        cancelText: qsTr("Cancel")
        onAccepted: root.performQuit()
    }
}
