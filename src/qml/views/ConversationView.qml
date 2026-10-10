import QtQuick
import Lib.Controls
import Lib.Theme

ScrollingArea {
    id: control

    interactionDomain: "app.conversation"
    contentCursorShape: Qt.IBeamCursor
    contentCursorStart: control.columnInset
    contentCursorWidth: control.columnWidth
    barTopMargin: Theme.metric.spacing
    barBottomMargin: Theme.metric.spacing

    property var chatController: null
    property real columnWidth: 0
    property real columnInset: 0

    function snapToBottom() {
        control.stickToBottom = true
        control.snapToEnd()
    }

    Item {
        width: control.columnWidth
        x: (control.width - width) / 2
        height: threadColumn.height

        Column {
            id: threadColumn
            width: parent.width

            Repeater {
                id: blockRepeater
                model: control.chatController ? control.chatController.conversation : null

                onItemAdded: {
                    if (control.stickToBottom) {
                        control.snapToEnd()
                    }
                }

                delegate: ConversationBlock {
                    width: threadColumn.width
                }
            }
        }
    }
}
