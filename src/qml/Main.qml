import QtQuick
import QtQuick.Window
import QtQuick.Controls
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

    property string pendingDelete: ""

    ChatController {
        id: chat
    }

    TopBar {
        id: topBar
        anchors.top: parent.top
        width: parent.width
        z: 9
        menuActive: appMenu.opened
        onMenuRequested: appMenu.toggle()
        onCloseRequested: Qt.quit()
    }

    BottomBar {
        id: bottomBar
        anchors.bottom: parent.bottom
        width: parent.width
        z: 9

        IconButton {
            id: historyButton
            glyph: "\uD83D\uDDE8\uFE0E"
            glyphColor: leftDrawer.width > 0 ? Theme.color.accent : Theme.color.textMuted
            x: Theme.metric.spacing
            anchors.verticalCenter: parent.verticalCenter
            onClicked: leftDrawer.size = leftDrawer.width > 0 ? 0 : 220
        }

        IconButton {
            id: inputDrawerButton
            glyph: "\u2328"
            glyphColor: inputDrawer.size > 0 ? Theme.color.accent : Theme.color.textMuted
            x: Math.max(content.x + Theme.metric.spacing, historyButton.x + historyButton.width + Theme.metric.spacing)
            anchors.verticalCenter: parent.verticalCenter
            onClicked: inputDrawer.size = inputDrawer.size > 0 ? 0 : 120
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

        ListView {
            id: conversationView
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: inputDrawer.top
            anchors.topMargin: Theme.metric.spacing
            anchors.leftMargin: Theme.metric.spacing
            anchors.rightMargin: Theme.metric.spacing
            clip: true
            spacing: 0
            model: chat.conversation
            onCountChanged: positionViewAtEnd()

            footer: Item {
                width: conversationView.width
                height: Theme.metric.spacing
            }

            delegate: Rectangle {
                width: ListView.view.width
                height: blockText.implicitHeight + 16
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

                Text {
                    id: blockText
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 8
                    text: model.kind === ConversationModel.ToolResult && model.payload.length > 0
                        ? model.text + " " + model.payload
                        : (model.kind === ConversationModel.ToolCall ? model.text + " " + model.payload : model.text)
                    color: model.kind === ConversationModel.Reasoning ? Theme.color.textMuted : Theme.color.text
                    font.pixelSize: 12
                    font.italic: model.kind === ConversationModel.Reasoning
                    font.family: (model.kind === ConversationModel.ToolCall || model.kind === ConversationModel.ToolResult)
                        ? "monospace" : ""
                    wrapMode: Text.Wrap
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
                    glyph: "\u27A4"
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.metric.spacing
                    anchors.verticalCenter: parent.verticalCenter
                    enabled: !chat.running && promptInput.text.trim().length > 0
                    onClicked: {
                        chat.sendMessage(promptInput.text)
                        promptInput.text = ""
                    }
                }

                IconButton {
                    id: stopButton
                    glyph: "\u25A0"
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
                onAccepted: {
                    chat.sendMessage(text)
                    text = ""
                }
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
                    glyph: "\u22EF"
                    active: historyMenu.opened
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: historyMenu.toggle()
                }

                IconButton {
                    id: addSessionButton
                    glyph: "+"
                    anchors.right: clearHistoryButton.left
                    anchors.verticalCenter: parent.verticalCenter
                    onClicked: {
                        chat.createSession()
                        inputDrawer.size = 72
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
                    width: ListView.view.width
                    height: 28
                    color: model.sessionId === chat.activeSession ? Theme.color.hover : "transparent"

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: chat.selectSession(model.sessionId)
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: deleteSessionButton.left
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: model.title
                        color: Theme.color.text
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }

                    IconButton {
                        id: deleteSessionButton
                        glyph: "\u2212"
                        anchors.right: parent.right
                        anchors.rightMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: {
                            root.pendingDelete = model.sessionId
                            deleteDialog.open()
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
            onClicked: Qt.quit()
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
}
