import QtQuick
import Lib.Controls
import Lib.Theme

Item {
    id: control

    property var chatController: null
    property real columnWidth: 0
    property real columnInset: 0
    property var imageLoader: null
    property var markdownParser: null
    property var customBlockRenderers: ({})
    property bool reasoningVisible: true
    property int userTextKind: 0
    property int assistantTextKind: 1
    property int reasoningKind: 2
    property int toolCallKind: 3
    property int toolResultKind: 4
    property int errorKind: 5
    property alias stickToBottom: scrollArea.stickToBottom

    readonly property alias contentY: scrollArea.contentY
    readonly property alias contentHeight: scrollArea.contentHeight
    readonly property alias viewportHeight: scrollArea.viewportHeight

    function snapToBottom() {
        control.stickToBottom = true
        scrollArea.snapToEnd()
    }

    function snapToEnd() {
        scrollArea.snapToEnd()
    }

    function atBottom() {
        return scrollArea.atBottom()
    }

    function scrollBy(delta) {
        scrollArea.scrollBy(delta)
    }

    Row {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        z: 2

        Item {
            width: Math.max(0, parent.width - reasoningButton.width - Theme.metric.spacing)
            height: parent.height
        }

        IconButton {
            id: reasoningButton
            objectName: "reasoningButton"
            iconName: control.reasoningVisible ? "eye" : "eye-off"
            accessibleLabel: control.reasoningVisible ? qsTr("Hide reasoning blocks") : qsTr("Show reasoning blocks")
            active: !control.reasoningVisible
            anchors.verticalCenter: parent.verticalCenter
            onClicked: control.reasoningVisible = !control.reasoningVisible
        }
    }

    ScrollingArea {
        id: scrollArea
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        interactionDomain: "app.conversation"
        contentCursorShape: Qt.IBeamCursor
        contentCursorStart: control.columnInset
        contentCursorWidth: control.columnWidth
        barTopMargin: Theme.metric.spacing
        barBottomMargin: Theme.metric.spacing
        stickToBottom: true

        Item {
            width: Math.min(control.columnWidth, scrollArea.width)
            x: (scrollArea.width - width) / 2
            height: threadColumn.height

            Column {
                id: threadColumn
                width: parent.width
                spacing: 0

                Repeater {
                    id: blockRepeater
                    model: control.chatController ? control.chatController.conversation : null

                    onItemAdded: {
                        if (control.stickToBottom) {
                            scrollArea.snapToEnd()
                        }
                    }

                    delegate: Item {
                        required property int index
                        required property int kind
                        required property string text
                        required property string payload

                        width: threadColumn.width
                        height: kind === control.reasoningKind && !control.reasoningVisible ? 0 : block.height

                        ConversationBlock {
                            id: block
                            width: threadColumn.width
                            index: parent.index
                            kind: parent.kind
                            text: parent.text
                            payload: parent.payload
                            visible: parent.kind !== control.reasoningKind || control.reasoningVisible
                            renderEnabled: control.visible && visible
                            streaming: control.chatController
                                && control.chatController.running
                                && parent.index === control.chatController.conversation.count - 1
                                && parent.kind === control.assistantTextKind
                            imageLoader: control.imageLoader
                            markdownParser: control.markdownParser
                            customBlockRenderers: control.customBlockRenderers
                            userTextKind: control.userTextKind
                            assistantTextKind: control.assistantTextKind
                            reasoningKind: control.reasoningKind
                            toolCallKind: control.toolCallKind
                            toolResultKind: control.toolResultKind
                            errorKind: control.errorKind
                        }
                    }
                }
            }
        }
    }
}
