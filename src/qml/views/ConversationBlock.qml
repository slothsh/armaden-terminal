import QtQuick
import Lib.Theme

Rectangle {
    id: block

    objectName: "conversationBlock"
    property int index: -1
    property int kind: 0
    property string text: ""
    property string payload: ""
    property bool reasoningVisible: true
    property bool streaming: false
    property bool renderEnabled: true
    property var imageLoader: null
    property var markdownParser: null
    property var scrollController: null
    property var customBlockRenderers: ({})
    property int userTextKind: 0
    property int assistantTextKind: 1
    property int reasoningKind: 2
    property int toolCallKind: 3
    property int toolResultKind: 4
    property int errorKind: 5
    property int toolResultPreviewLimit: 120

    function resetTransientState() {
        toolResultCode.expanded = false
        toolResultCode.wordWrap = true
    }

    onIndexChanged: block.resetTransientState()
    onKindChanged: block.resetTransientState()
    onTextChanged: block.resetTransientState()
    onPayloadChanged: block.resetTransientState()

    function decodeJsonValue(value) {
        let decoded = value
        for (let depth = 0; depth < 2 && typeof decoded === "string"; ++depth) {
            try {
                decoded = JSON.parse(decoded)
            } catch (error) {
                break
            }
        }

        return decoded
    }

    function scrollTextWheel(wheel) {
        if (!block.scrollController) {
            return
        }

        const delta = wheel.pixelDelta.y !== 0
            ? wheel.pixelDelta.y
            : wheel.angleDelta.y / 120 * block.scrollController.wheelStep
        block.scrollController.scrollBy(-delta)
        wheel.accepted = true
    }

    function parseToolResult(value) {
        try {
            return {valid: true, value: block.decodeJsonValue(JSON.parse(value))}
        } catch (error) {
            return {valid: false, value: null}
        }
    }

    function isMessageEnvelope(value) {
        if (value === null || typeof value !== "object" || Array.isArray(value)) {
            return false
        }
        if (!Object.prototype.hasOwnProperty.call(value, "content")) {
            return false
        }

        return Object.prototype.hasOwnProperty.call(value, "role")
            || Object.prototype.hasOwnProperty.call(value, "type")
            || Object.prototype.hasOwnProperty.call(value, "tool_call_id")
            || Object.prototype.hasOwnProperty.call(value, "content_blocks")
            || Object.prototype.hasOwnProperty.call(value, "name")
    }

    function toolResultReturnedValue(value) {
        if (value === null || typeof value !== "object" || Array.isArray(value)) {
            return value
        }

        if (block.isMessageEnvelope(value)) {
            if (typeof value.content === "string" && value.content.length > 0) {
                return block.decodeJsonValue(value.content)
            }
            const blocks = value.content_blocks
            if (Array.isArray(blocks) && blocks.length > 0 && typeof blocks[0].text === "string") {
                return block.decodeJsonValue(blocks[0].text)
            }

            return value.content === undefined || value.content === null ? "" : value.content
        }

        const hasRequest = Object.prototype.hasOwnProperty.call(value, "request")
        const hasResponse = Object.prototype.hasOwnProperty.call(value, "response")
        if (!hasRequest && !hasResponse) {
            return value
        }

        const result = {}
        if (hasRequest) {
            result.request = block.decodeJsonValue(value.request)
        }
        if (hasResponse) {
            result.response = block.decodeJsonValue(value.response)
        }

        return result
    }

    readonly property var parsedToolResult: block.kind === block.toolResultKind
        ? block.parseToolResult(block.payload)
        : ({valid: false, value: null})
    readonly property var returnedToolResult: block.parsedToolResult.valid
        ? block.toolResultReturnedValue(block.parsedToolResult.value)
        : null
    readonly property string toolResultText: typeof block.returnedToolResult === "string"
        ? block.returnedToolResult : ""
    readonly property bool hasJsonToolResult: block.parsedToolResult.valid
        && block.returnedToolResult !== null
        && typeof block.returnedToolResult === "object"
    readonly property string toolResultFullCode: block.hasJsonToolResult
        ? JSON.stringify(block.returnedToolResult, null, 2)
        : ""
    readonly property string toolResultPreview: {
        const source = block.toolResultText.length > 0 ? block.toolResultText : block.payload
        const compact = String(source).replace(/\s+/g, " ").trim()
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
    readonly property bool plainContent: !block.markdownContent && !block.hasJsonToolResult

    height: block.kind === block.reasoningKind && !block.reasoningVisible ? 0 : body.implicitHeight + 24
    visible: block.kind !== block.reasoningKind || block.reasoningVisible
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
        implicitHeight: block.markdownContent
            ? markdown.implicitHeight
            : (block.hasJsonToolResult ? toolResultCode.implicitHeight : plainTextLoader.implicitHeight)
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
            scrollController: block.scrollController
            customBlockRenderers: block.customBlockRenderers
            contentColor: block.kind === block.reasoningKind ? Theme.color.textMuted : Theme.color.text
            mutedColor: Theme.color.textMuted
        }

        CodeBlock {
            id: toolResultCode
            objectName: "toolResultCode"
            width: body.width
            visible: block.hasJsonToolResult
            code: block.toolResultFullCode
            language: "json"
            collapsedHeight: 180
        }

        Loader {
            id: plainTextLoader
            width: body.width
            active: block.plainContent
            sourceComponent: plainContentComponent
        }

        Component {
            id: plainContentComponent

            Item {
                width: plainTextLoader.width
                implicitHeight: plainText.contentHeight
                height: implicitHeight

                TextEdit {
                    id: plainText
                    objectName: "conversationPlainText"
                    width: parent.width
                    height: Math.max(contentHeight, font.pixelSize * 1.4)
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

                    HoverHandler {
                        objectName: "conversationTextCursor"
                        cursorShape: Qt.IBeamCursor
                    }
                }

                MouseArea {
                    objectName: "conversationTextWheelCapture"
                    anchors.fill: plainText
                    acceptedButtons: Qt.NoButton
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    onWheel: wheel => block.scrollTextWheel(wheel)
                }
            }
        }
    }
}
