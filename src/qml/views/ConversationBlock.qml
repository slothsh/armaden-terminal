import QtQuick
import Lib.Theme

Rectangle {
    id: block

    required property int index
    required property int kind
    required property string text
    required property string payload

    width: parent.width
    height: blockText.contentHeight + 24
    radius: 0
    color: {
        switch (block.kind) {
            case ConversationModel.UserText: return Theme.color.medium
            case ConversationModel.Reasoning: return Theme.color.menu
            case ConversationModel.ToolCall: return Theme.color.menu
            case ConversationModel.ToolResult: return Theme.color.menu
            case ConversationModel.Error: return "#3a1f1f"
            default: return Theme.color.menu
        }
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        visible: block.index > 0
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
        text: block.kind === ConversationModel.ToolResult && block.payload.length > 0
            ? block.text + " " + block.payload
            : (block.kind === ConversationModel.ToolCall ? block.text + " " + block.payload : block.text)
        color: block.kind === ConversationModel.Reasoning ? Theme.color.textMuted : Theme.color.text
        font.pixelSize: 12
        font.italic: block.kind === ConversationModel.Reasoning
        font.family: (block.kind === ConversationModel.ToolCall || block.kind === ConversationModel.ToolResult)
            ? "monospace" : ""
        selectionColor: Theme.color.accent
        selectedTextColor: Theme.color.text
    }
}
