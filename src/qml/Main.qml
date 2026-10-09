import QtQuick
import QtQuick as Quick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Templates as T
import QtQuick.Effects
import Lib.Controls
import Lib.Theme

Window {
    id: root
    visible: true
    color: Theme.color.dark
    width: 960
    height: 640
    title: qsTr("mindtosis")

    property var chat: null
    property string pendingDelete: ""
    property string renamingSession: ""

    signal contextMenuRequested(real x, real y)

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
        if (root.renamingSession !== id)
            return
        root.renamingSession = ""
        const trimmed = title.trim()
        if (trimmed.length > 0)
            chat.renameSession(id, trimmed)
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
        expandVisible: chat.conversation.count > 0
        expandActive: root.chatExpanded
        expandEnabled: !root.chatNarrow
        contentX: content.x
        contentInset: root.chatColumnInset
        onMenuRequested: appMenu.toggle()
        onCloseRequested: root.requestQuit()
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
            iconColor: inputDrawer.size > 0 ? Theme.color.accent : Theme.color.textMuted
            x: Math.max(content.x + Theme.metric.spacing, historyButton.x + historyButton.width + Theme.metric.spacing)
            anchors.verticalCenter: parent.verticalCenter
            onClicked: inputDrawer.toggle()
        }

        IconButton {
            id: mapPaneButton
            iconName: "map"
            iconColor: root.mapPaneVisible ? Theme.color.accent : Theme.color.textMuted
            x: Math.max(content.x + Theme.metric.spacing, inputDrawerButton.x + inputDrawerButton.width + Theme.metric.spacing)
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.mapPaneVisible = !root.mapPaneVisible
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
        x: leftDrawer.width
        y: topBar.height
        width: parent.width - leftDrawer.width
        height: parent.height - topBar.height - bottomBar.height
        color: Theme.color.dark
        T.ContextMenu.menu: null
        T.ContextMenu.onRequested: function(position) {
            root.contextMenuRequested(position.x, position.y)
        }

        MapCanvas {
            id: mapCanvas
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
                    interactive: !chat.running && promptInput.text.trim().length > 0
                    onClicked: {
                        conversationView.stickToBottom = true
                        chat.sendMessage(promptInput.text)
                        promptInput.text = ""
                    }
                }

                IconButton {
                    id: stopButton
                    iconName: "square"
                    anchors.left: sendButton.right
                    anchors.leftMargin: Theme.metric.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    visible: chat.running
                    onClicked: chat.cancel()
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
                anchors.top: inputGutter.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                placeholderText: qsTr("Message the agent...")
                readOnly: chat.running
                onTextChanged: draftSaveTimer.restart()
                Component.onCompleted: text = chat.draft
                onAccepted: {
                    conversationView.stickToBottom = true
                    chat.sendMessage(text)
                    text = ""
                }
                onContextMenuRequested: (x, y) => root.contextMenuRequested(x, y)
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
                x: leftDrawer.width - width
                y: historyGutter.y + clearHistoryButton.y + clearHistoryButton.height + 2

                MenuButton {
                    width: historyMenu.width - historyMenu.padding * 2
                    label: qsTr("Clear history")
                    onClicked: {
                        historyMenu.close()
                        clearDialog.open()
                    }
                }
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

                    readonly property bool renaming: root.renamingSession === model.sessionId
                    readonly property bool menuEngaged: sessionMenu.opened && !sessionMenuButton.openedByButton
                    color: model.sessionId === chat.activeSession
                        ? Theme.color.active
                        : ((rowHover.hovered || menuEngaged) ? Theme.color.hover : "transparent")

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
                                sessionMenuButton.openedByButton = false
                                sessionMenu.openAt(mouse.x, mouse.y)
                                return
                            }
                            conversationView.stickToBottom = true
                            chat.saveDraft(promptInput.text)
                            chat.selectSession(model.sessionId)
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

                        onAccepted: root.commitRename(model.sessionId, text)

                        onActiveFocusChanged: {
                            if (!activeFocus && sessionRow.renaming) {
                                root.commitRename(model.sessionId, text)
                            }
                        }

                        Keys.onEscapePressed: event => {
                            root.renamingSession = ""
                            event.accepted = true
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
                        property bool openedByButton: false
                        active: sessionMenu.opened && sessionMenuButton.openedByButton
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: {
                            sessionMenuButton.openedByButton = true
                            sessionMenu.x = sessionRow.width - sessionMenu.width
                            sessionMenu.y = sessionRow.height + 2
                            sessionMenu.toggle()
                        }
                    }

                    ContextMenu {
                        id: sessionMenu

                        MenuButton {
                            width: sessionMenu.width - sessionMenu.padding * 2
                            label: qsTr("Rename")
                            onClicked: {
                                sessionMenu.close()
                                root.renamingSession = model.sessionId
                            }
                        }

                        MenuButton {
                            width: sessionMenu.width - sessionMenu.padding * 2
                            label: qsTr("Remove")
                            onClicked: {
                                sessionMenu.close()
                                root.pendingDelete = model.sessionId
                                deleteDialog.open()
                            }
                        }
                    }
                }
            }
        }
    }

    DropdownMenu {
        id: appMenu
        x: Theme.metric.spacing
        y: topBar.menuAnchorBottom + 2

        MenuButton {
            width: appMenu.width - appMenu.padding * 2
            label: qsTr("About")
            onClicked: {
                appMenu.close()
                aboutDialog.open()
            }
        }

        Rectangle {
            width: appMenu.width - appMenu.padding * 2
            height: 9
            color: "transparent"

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 1
                color: "#1affffff"
            }
        }

        MenuButton {
            width: appMenu.width - appMenu.padding * 2
            label: qsTr("Quit")
            onClicked: {
                appMenu.close()
                root.requestQuit()
            }
        }
    }

    ConfirmDialog {
        id: deleteDialog
        message: qsTr("Are you sure you want to delete this session's history?")
        onAccepted: chat.deleteSession(root.pendingDelete)
    }

    ConfirmDialog {
        id: clearDialog
        message: qsTr("Are you sure you want to clear all conversation history?")
        onAccepted: chat.clearHistory()
    }

    AboutDialog {
        id: aboutDialog
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
        message: qsTr("Are you sure you want to exit? Any in-flight responses will be cancelled.")
        confirmText: qsTr("Quit")
        cancelText: qsTr("Cancel")
        onAccepted: root.performQuit()
    }
}
