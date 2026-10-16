import QtQuick
import Lib.Controls
import Lib.Interaction
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

    onReasoningVisibleChanged: control.invalidateReasoningSizes()

    function invalidateReasoningSizes() {
        if (!control.chatController) {
            return
        }

        const conversation = control.chatController.conversation
        for (let row = 0; row < conversation.count; ++row) {
            if (conversation.kindAt(row) === control.reasoningKind) {
                listView.invalidateSize(row)
            }
        }
    }
    property bool expandVisible: true
    property bool expandActive: false
    property bool expandEnabled: true
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

    signal expandRequested()

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

    Item {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        z: 2

        IconButton {
            id: expandButton
            objectName: "conversationExpandButton"
            iconName: "expand"
            accessibleLabel: qsTr("Expand conversation")
            interactionDomain: "app.conversation"
            interactionGroup: "conversation-bar"
            interactionOrder: 1
            x: Math.max(control.columnInset, Theme.metric.spacing)
            anchors.verticalCenter: parent.verticalCenter
            visible: control.expandVisible
            enabled: control.expandEnabled
            iconColor: control.expandActive ? Theme.color.accent : Theme.color.textMuted
            onClicked: control.expandRequested()
        }

        IconButton {
            id: reasoningButton
            objectName: "reasoningButton"
            iconName: control.reasoningVisible ? "eye" : "eye-off"
            accessibleLabel: control.reasoningVisible ? qsTr("Hide reasoning blocks") : qsTr("Show reasoning blocks")
            interactionDomain: "app.conversation"
            interactionGroup: "conversation-bar"
            interactionOrder: 2
            active: !control.reasoningVisible
            anchors.right: parent.right
            anchors.rightMargin: Theme.metric.spacing
            anchors.verticalCenter: parent.verticalCenter
            onClicked: control.reasoningVisible = !control.reasoningVisible
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.color.borderStrong
        }
    }

    ScrollingArea {
        id: scrollArea
        objectName: "conversationScrollArea"
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        interactionDomain: "app.conversation"
        barTopMargin: Theme.metric.spacing
        barBottomMargin: Theme.metric.spacing
        stickToBottom: true
        contentHeightHint: listView.contentHeight

        VirtualListView {
            id: listView
            objectName: "conversationListView"
            width: Math.min(control.columnWidth, scrollArea.width)
            x: (scrollArea.width - width) / 2
            height: listView.contentHeight

            model: control.chatController ? control.chatController.conversation : null
            contentY: scrollArea.contentY
            viewportHeight: scrollArea.viewportHeight
            cacheBuffer: 320
            estimatedItemHeight: 96
            maxItemsPerFrame: 8
            followTail: scrollArea.stickToBottom && scrollArea.atBottom()

            sizeHint: function(index, width, model) {
                if (model && model.kindAt(index) === control.reasoningKind && !control.reasoningVisible) {
                    return 1
                }
                return model ? model.heightHint(index, width) : 96
            }

            onScrollAdjustmentRequested: delta => scrollArea.adjustBy(delta)

            delegate: Component {
                ConversationBlock {
                    id: block

                    reasoningVisible: control.reasoningVisible
                    renderEnabled: control.visible
                    streaming: control.chatController
                        && control.chatController.running
                        && block.index === control.chatController.conversation.count - 1
                        && block.kind === control.assistantTextKind
                    imageLoader: control.imageLoader
                    markdownParser: control.markdownParser
                    customBlockRenderers: control.customBlockRenderers
                    scrollController: scrollArea
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
