import QtQuick
import Lib.Theme

Rectangle {
    id: block

    required property int index
    required property int kind
    required property string text
    required property string payload
    property bool streaming: false
    property bool renderEnabled: true
    property var imageLoader: null
    property var markdownParser: null
    property var customBlockRenderers: ({})
    property int userTextKind: 0
    property int assistantTextKind: 1
    property int reasoningKind: 2
    property int toolCallKind: 3
    property int toolResultKind: 4
    property int errorKind: 5
    property int toolResultPreviewLimit: 120

    readonly property string toolResultPreview: {
        const compact = String(block.payload).replace(/\s+/g, " ").trim()
        if (compact.length <= block.toolResultPreviewLimit) {
            return compact
        }

        return compact.slice(0, Math.max(0, block.toolResultPreviewLimit - 1)).trim() + "…"
    }
    readonly property string displayText: {
        if (block.kind === block.toolCallKind) {
            const name = block.text.trim()
            return name.length > 0 ? qsTr("Used tool: %1").arg(name) : qsTr("Used a tool")
        }
        if (block.kind === block.toolResultKind) {
            return block.toolResultPreview.length > 0
                ? qsTr("Result: %1").arg(block.toolResultPreview)
                : qsTr("Result: (empty)")
        }
        return block.text
    }

    readonly property bool markdownContent: block.kind === block.assistantTextKind
        || block.kind === block.reasoningKind

    width: parent.width
    height: body.implicitHeight + 24
    radius: 0
    color: {
        switch (block.kind) {
            case block.userTextKind: return Theme.color.medium
            case block.reasoningKind: return Theme.color.menu
            case block.toolCallKind: return Theme.color.menu
            case block.toolResultKind: return Theme.color.menu
            case block.errorKind: return "#3a1f1f"
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

    Item {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        implicitHeight: block.markdownContent ? markdown.implicitHeight : plainText.contentHeight
        height: implicitHeight

        MarkdownView {
            id: markdown
            width: body.width
            visible: block.markdownContent
            sourceText: block.text
            streaming: block.streaming
            renderEnabled: block.renderEnabled
            parser: block.markdownParser
            imageLoader: block.imageLoader
            customBlockRenderers: block.customBlockRenderers
            contentColor: block.kind === block.reasoningKind ? Theme.color.textMuted : Theme.color.text
            mutedColor: Theme.color.textMuted
        }

        TextEdit {
            id: plainText
            objectName: "conversationPlainText"
            width: body.width
            height: Math.max(contentHeight, font.pixelSize * 1.4)
            visible: !block.markdownContent
            readOnly: true
            selectByMouse: true
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            text: block.displayText
            color: block.kind === block.reasoningKind ? Theme.color.textMuted : Theme.color.text
            font.pixelSize: 12
            font.italic: false
            font.family: (block.kind === block.toolCallKind || block.kind === block.toolResultKind)
                ? "monospace" : ""
            selectionColor: Theme.color.accent
            selectedTextColor: Theme.color.text
        }
    }
}
