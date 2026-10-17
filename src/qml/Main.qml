import QtQuick
import QtQuick.Templates as T
import Lib.Controls
import Lib.Interaction
import Lib.Theme
import Application
import "views"

Item {
    id: root

    InteractionScope.domain: "app"
    InteractionScope.role: "window"
    InteractionScope.activatable: true

    property var chat: null
    property var interaction: null
    property var markdownImages: null
    property var markdownParser: null
    property string pendingDelete: ""
    property string renamingSession: ""

    signal contextMenuRequested(real x, real y)

    onContextMenuRequested: (x, y) => {
        root.interaction.executeCommand({
            kind: "openContextMenu",
            target: root.interaction.hoveredTarget,
            x: x,
            y: y,
        })
    }

    property bool mapPaneVisible: false
    property bool chatExpanded: false
    readonly property real chatPageWidth: 960
    readonly property real chatMarginMin: Theme.metric.spacing
    readonly property bool chatNarrow: content.width < chatPageWidth + chatMarginMin * 2
    readonly property bool chatFullWidth: chatExpanded || chatNarrow
    readonly property real chatColumnInset: chatFullWidth ? 0 : Math.round((content.width - chatPageWidth) / 2)
    readonly property real chatColumnWidth: chatFullWidth ? content.width : chatPageWidth

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
        conversationView.snapToBottom()
        chat.sendMessage(promptPane.currentText())
        promptPane.clearInput()
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
                quitController.requestQuit()
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
                if (!root.mapPaneVisible) {
                    promptPane.open()
                }
                break
            }
        }
    }

    Component.onCompleted: {
        interaction.setBusinessValue("chat.running", chat.running ? "true" : "false")
        interaction.setBusinessValue("prompt.text", promptPane.currentText().trim())
        interaction.setBusinessValue("map.ready", chat.mapController.ready ? "true" : "false")
    }

    function interceptClose(close) {
        if (!quitController.quitConfirmed) {
            close.accepted = false
            quitController.requestQuit()
        }
    }

    Item {
        id: overlayItem
        anchors.fill: parent
        z: 99
    }

    QuitController {
        id: quitController
        anchors.fill: parent
        chatController: chat
        interactionController: root.interaction
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
        contentX: content.x
        onMenuRequested: appMenu.toggle()
        onCloseRequested: interaction.invokeAction("app.quit", root.InteractionScope.targetId)
    }

    BottomBar {
        id: bottomBar
        anchors.bottom: parent.bottom
        width: parent.width
        z: 9
        interactionController: root.interaction
        actionTarget: root.InteractionScope.targetId
        contentX: content.x
        historyActive: sessionPane.drawerWidth > 0
        inputActive: !root.mapPaneVisible && promptPane.drawerSize > 0
        mapActive: root.mapPaneVisible
        onHistoryRequested: sessionPane.toggleHistory()
        onInputDrawerRequested: {
            if (root.mapPaneVisible) {
                root.interaction.invokeAction("map.toggle", root.InteractionScope.targetId)
                return
            }

            promptPane.toggle()
        }
        onMapToggleRequested: root.interaction.invokeAction("map.toggle", root.InteractionScope.targetId)
    }

    Rectangle {
        id: content
        InteractionScope.domain: "app.main"
        InteractionScope.role: "pane"
        x: sessionPane.drawerWidth
        y: topBar.height
        width: parent.width - sessionPane.drawerWidth
        height: parent.height - topBar.height - bottomBar.height
        color: Theme.color.dark
        T.ContextMenu.menu: null
        T.ContextMenu.onRequested: function(position) {
            const point = content.mapToItem(overlayItem, position.x, position.y)
            root.contextMenuRequested(point.x, point.y)
        }

        MapCanvas {
            id: mapCanvas
            InteractionScope.domain: "app.map"
            InteractionScope.role: "surface"
            InteractionScope.label: qsTr("Map canvas")
            InteractionScope.group: "map-camera"
            InteractionScope.order: 0
            InteractionScope.hitTestable: true
            InteractionScope.contextMenuable: true
            InteractionScope.focusable: true
            InteractionScope.activatable: true
            InteractionScope.navigable: true
            InteractionScope.draggable: true
            InteractionScope.scrollable: true
            InteractionScope.pannable: true
            InteractionScope.zoomable: true
            anchors.fill: parent
            visible: root.mapPaneVisible
            controller: chat.mapController
        }

        ConversationView {
            id: conversationView
            visible: !root.mapPaneVisible
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: promptPane.top
            chatController: chat
            imageLoader: root.markdownImages
            markdownParser: root.markdownParser
            userTextKind: ConversationModel.UserText
            assistantTextKind: ConversationModel.AssistantText
            reasoningKind: ConversationModel.Reasoning
            toolCallKind: ConversationModel.ToolCall
            toolResultKind: ConversationModel.ToolResult
            errorKind: ConversationModel.Error
            columnWidth: root.chatColumnWidth
            columnInset: root.chatColumnInset
            expandVisible: !root.mapPaneVisible && chat.conversation.count > 0
            expandActive: root.chatExpanded
            expandEnabled: !root.chatNarrow
            onExpandRequested: root.chatExpanded = !root.chatExpanded
        }

        PromptPane {
            id: promptPane
            visible: !root.mapPaneVisible
            chatActive: !root.mapPaneVisible
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            chatController: chat
            interactionController: root.interaction
            contentHeight: content.height
            overlay: overlayItem
            onContextMenuRequested: (x, y) => root.contextMenuRequested(x, y)
        }
    }

    SessionPane {
        id: sessionPane
        width: root.width
        height: root.height
        chatController: chat
        interactionController: root.interaction
        renamingSession: root.renamingSession
        topBarHeight: topBar.height
        bottomBarHeight: bottomBar.height
        contextMenu: interactionContextMenu
        overlay: overlayItem
        onCreateSessionRequested: {
            conversationView.snapToBottom()
            promptPane.saveDraft()
            chat.createSession()
        }
        onRenameCancelled: root.renamingSession = ""
        onRenameAccepted: (sessionId, title) => root.commitRename(sessionId, title)
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

        function onSelectionCommandRequested(domain, cursor) {
            if (domain !== "app.sessionList" || cursor === 0) {
                return
            }

            const sessionId = root.interaction.targetSemanticKey(cursor)
            if (sessionId.length === 0) {
                return
            }

            conversationView.snapToBottom()
            promptPane.saveDraft()
            chat.selectSession(sessionId)
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


}
