import QtQuick
import Lib.Theme

Item {
    id: control

    property var node: ({kind: "document", children: []})
    property var imageLoader: null
    property var customBlockRenderers: ({})
    property var scrollController: null
    property int imageRevision: 0
    property string parentKind: ""
    property int position: 0
    property int listStart: 1
    property string listMarker: "•"
    property int tableColumns: 1
    property bool renderEnabled: true
    property color contentColor: Theme.color.text
    property color mutedColor: Theme.color.textMuted

    signal linkActivated(string link)

    implicitHeight: nodeRenderer.item ? nodeRenderer.item.implicitHeight : 0
    height: implicitHeight

    function scrollTextWheel(wheel) {
        if (!control.scrollController) {
            return
        }

        const delta = wheel.pixelDelta.y !== 0
            ? wheel.pixelDelta.y
            : wheel.angleDelta.y / 120 * control.scrollController.wheelStep
        control.scrollController.scrollBy(-delta)
        wheel.accepted = true
    }

    function escapeHtml(value) {
        return String(value)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/\"/g, "&quot;")
            .replace(/'/g, "&#39;")
    }

    function safeLink(value) {
        const url = String(value).trim()
        const lower = url.toLowerCase()
        if (lower.startsWith("https://") || lower.startsWith("http://") || lower.startsWith("mailto:") || lower.startsWith("#")) {
            return url
        }
        return ""
    }

    function inlineHtml(nodes) {
        let result = ""
        for (let i = 0; i < nodes.length; ++i) {
            result += inlineNodeHtml(nodes[i])
        }
        return result
    }

    function inlineNodeHtml(item) {
        const imageRevision = control.imageRevision
        const children = item.children || []
        const value = escapeHtml(item.text || "")
        switch (item.kind) {
            case "text": return value
            case "entity": return item.text || ""
            case "hardBreak": return "<br/>"
            case "softBreak": return " "
            case "emphasis": return "<em>" + inlineHtml(children) + "</em>"
            case "strong": return "<strong>" + inlineHtml(children) + "</strong>"
            case "strikethrough": return "<s>" + inlineHtml(children) + "</s>"
            case "inlineCode": return "<code style=\"font-family:monospace;background-color:" + Theme.color.medium + ";\">" + value + "</code>"
            case "link": {
                const href = safeLink(item.destination || "")
                return href.length > 0
                    ? "<a href=\"" + escapeHtml(href) + "\">" + inlineHtml(children) + "</a>"
                    : inlineHtml(children)
            }
            case "image": return "<span style=\"color:" + control.mutedColor + ";\">[Image: " + value + "]</span>"
            case "rawHtml": return value
            default: return children.length > 0 ? inlineHtml(children) : value
        }
    }

    function richTextHtml(item) {
        if (item.kind === "text" || item.kind === "entity" || item.kind === "emphasis"
                || item.kind === "strong" || item.kind === "link" || item.kind === "image"
                || item.kind === "inlineCode" || item.kind === "strikethrough"
                || item.kind === "hardBreak" || item.kind === "softBreak") {
            return inlineNodeHtml(item)
        }

        const body = inlineHtml(item.children || [])
        switch (item.kind) {
            case "heading": {
                const size = Math.max(1, Math.min(6, item.level || 1))
                return "<h" + size + " style=\"color:" + control.contentColor + ";\">" + body + "</h" + size + ">"
            }
            case "tableHeaderCell": return "<strong>" + body + "</strong>"
            case "rawHtml": return escapeHtml(item.text || "")
            default: return body
        }
    }

    function hasImage(item) {
        const children = item.children || []
        for (let i = 0; i < children.length; ++i) {
            if (children[i].kind === "image" || control.hasImage(children[i])) {
                return true
            }
        }
        return false
    }

    function tableColumnCount(item) {
        const rows = item.children || []
        for (let rowIndex = 0; rowIndex < rows.length; ++rowIndex) {
            if (rows[rowIndex].kind === "tableHead" || rows[rowIndex].kind === "tableBody") {
                const groupRows = rows[rowIndex].children || []
                if (groupRows.length > 0) {
                    return Math.max(1, (groupRows[0].children || []).length)
                }
            } else if (rows[rowIndex].kind === "tableRow") {
                return Math.max(1, (rows[rowIndex].children || []).length)
            }
        }
        return 1
    }

    function componentForNode() {
        if (node.kind === "codeBlock") {
            return codeComponent
        }
        if (node.kind === "image") {
            return imageComponent
        }
        if ((node.kind === "paragraph" || node.kind === "tableCell" || node.kind === "tableHeaderCell")
                && control.hasImage(node)) {
            return inlineContentComponent
        }
        if (node.kind === "horizontalRule") {
            return ruleComponent
        }
        if (node.kind === "listItem") {
            return listItemComponent
        }
        if (node.kind === "blockquote") {
            return quoteComponent
        }
        if (node.kind === "tableRow") {
            return tableRowComponent
        }
        if (node.kind === "paragraph" || node.kind === "heading" || node.kind === "tableCell" || node.kind === "tableHeaderCell"
                || node.kind === "text" || node.kind === "entity" || node.kind === "emphasis"
                || node.kind === "strong" || node.kind === "link" || node.kind === "image"
                || node.kind === "inlineCode" || node.kind === "strikethrough"
                || node.kind === "hardBreak" || node.kind === "softBreak") {
            return richTextComponent
        }
        if (node.kind === "customBlock" && customBlockRenderers && customBlockRenderers[node.renderer]) {
            return customBlockRenderers[node.renderer]
        }
        if (node.kind === "rawHtml") {
            return fallbackComponent
        }
        return containerComponent
    }

    Loader {
        id: nodeRenderer
        width: control.width
        sourceComponent: control.componentForNode()
        onLoaded: {
            if (control.node.kind === "customBlock" && item) {
                if ("node" in item) {
                    item.node = control.node
                }
                if ("imageLoader" in item) {
                    item.imageLoader = control.imageLoader
                }
            }
        }
    }

    Component {
        id: codeComponent

        CodeBlock {
            width: control.width
            code: control.node.text
            language: control.node.language
        }
    }

    Component {
        id: imageComponent

        MarkdownImage {
            width: Math.min(control.width, implicitWidth)
            sourceUrl: control.node.destination
            altText: control.node.text
            title: control.node.title
            maximumWidth: control.width
            imageLoader: control.imageLoader
            renderEnabled: control.renderEnabled
        }
    }

    Component {
        id: inlineContentComponent

        MarkdownInlineContent {
            width: control.width
            nodes: control.node.children || []
            imageLoader: control.imageLoader
            customBlockRenderers: control.customBlockRenderers
            scrollController: control.scrollController
            imageRevision: control.imageRevision
            contentColor: control.contentColor
            mutedColor: control.mutedColor
            renderEnabled: control.renderEnabled
        }
    }

    Component {
        id: ruleComponent

        Rectangle {
            width: control.width
            height: 1
            color: Theme.color.borderStrong
            implicitHeight: height
        }
    }

    Component {
        id: richTextComponent

        Item {
            width: control.width
            implicitHeight: Math.max(richText.contentHeight, richText.font.pixelSize * 1.4)
            height: implicitHeight

            TextEdit {
                id: richText
                objectName: "markdownRichText"
                anchors.fill: parent
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                text: control.richTextHtml(control.node)
                color: control.contentColor
                font.pixelSize: 12
                selectionColor: Theme.color.accent
                selectedTextColor: control.contentColor
                horizontalAlignment: control.node.alignment === 2
                    ? TextEdit.AlignHCenter
                    : (control.node.alignment === 3 ? TextEdit.AlignRight : TextEdit.AlignLeft)
                onLinkActivated: link => control.linkActivated(link)

                HoverHandler {
                    objectName: "markdownRichTextCursor"
                    cursorShape: Qt.IBeamCursor
                }

            }

            MouseArea {
                objectName: "markdownRichTextWheelCapture"
                anchors.fill: richText
                acceptedButtons: Qt.NoButton
                hoverEnabled: true
                cursorShape: Qt.IBeamCursor
                onWheel: wheel => control.scrollTextWheel(wheel)
            }
        }
    }

    Component {
        id: fallbackComponent

        Item {
            width: control.width
            implicitHeight: Math.max(fallbackText.contentHeight, fallbackText.font.pixelSize * 1.4)
            height: implicitHeight

            TextEdit {
                id: fallbackText
                anchors.fill: parent
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                text: control.node.text
                color: control.mutedColor
                font.pixelSize: 12
                selectionColor: Theme.color.accent
                selectedTextColor: control.contentColor

                HoverHandler {
                    objectName: "markdownFallbackTextCursor"
                    cursorShape: Qt.IBeamCursor
                }

            }

            MouseArea {
                objectName: "markdownFallbackTextWheelCapture"
                anchors.fill: fallbackText
                acceptedButtons: Qt.NoButton
                hoverEnabled: true
                cursorShape: Qt.IBeamCursor
                onWheel: wheel => control.scrollTextWheel(wheel)
            }
        }
    }

    Component {
        id: containerComponent

        Column {
            id: childColumn
            width: control.width
            spacing: control.node.kind === "document" ? 8 : 2

            Repeater {
                model: control.node.children || []

                delegate: MarkdownNodeDelegate {
                    width: childColumn.width
                    imageLoader: control.imageLoader
                    customBlockRenderers: control.customBlockRenderers
                    scrollController: control.scrollController
                    imageRevision: control.imageRevision
                    renderEnabled: control.renderEnabled
                    parentKind: control.node.kind
                    listStart: control.node.start || 1
                    listMarker: control.node.marker || "•"
                    tableColumns: control.node.kind === "table" ? control.tableColumnCount(control.node) : control.tableColumns
                    contentColor: control.contentColor
                    mutedColor: control.mutedColor
                    onLinkActivated: link => control.linkActivated(link)
                }
            }
        }
    }

    Component {
        id: listItemComponent

        Row {
            id: listItemRow
            width: control.width
            spacing: 4

            Text {
                width: 22
                text: control.node.task
                    ? (control.node.checked ? "☑" : "☐")
                    : (control.parentKind === "orderedList" ? (control.listStart + control.position) + "." : control.listMarker)
                color: control.node.task ? control.mutedColor : control.contentColor
                font.pixelSize: 12
                horizontalAlignment: Text.AlignRight
            }

            Column {
                id: listItemContent
                width: Math.max(0, listItemRow.width - 26)
                spacing: 2

                Repeater {
                    model: control.node.children || []

                    delegate: MarkdownNodeDelegate {
                        width: listItemContent.width
                        imageLoader: control.imageLoader
                        customBlockRenderers: control.customBlockRenderers
                        scrollController: control.scrollController
                        imageRevision: control.imageRevision
                        renderEnabled: control.renderEnabled
                        parentKind: "listItem"
                        listStart: control.listStart
                        listMarker: control.listMarker
                        tableColumns: control.tableColumns
                        contentColor: control.contentColor
                        mutedColor: control.mutedColor
                        onLinkActivated: link => control.linkActivated(link)
                    }
                }
            }
        }
    }

    Component {
        id: quoteComponent

        Row {
            id: quoteRow
            width: control.width
            spacing: 8

            Rectangle {
                width: 3
                height: quoteContent.implicitHeight
                color: Theme.color.borderStrong
            }

            Column {
                id: quoteContent
                width: Math.max(0, quoteRow.width - 11)
                spacing: 2

                Repeater {
                    model: control.node.children || []

                    delegate: MarkdownNodeDelegate {
                        width: quoteContent.width
                        imageLoader: control.imageLoader
                        customBlockRenderers: control.customBlockRenderers
                        scrollController: control.scrollController
                        imageRevision: control.imageRevision
                        renderEnabled: control.renderEnabled
                        parentKind: "blockquote"
                        tableColumns: control.tableColumns
                        contentColor: control.contentColor
                        mutedColor: control.mutedColor
                        onLinkActivated: link => control.linkActivated(link)
                    }
                }
            }
        }
    }

    Component {
        id: tableRowComponent

        Row {
            id: row
            width: control.width
            spacing: 0

            Repeater {
                model: control.node.children || []

                delegate: Item {
                    required property var modelData
                    required property int index

                    width: row.width / Math.max(1, control.node.children.length)
                    height: cellRenderer.implicitHeight + 12

                    MarkdownNodeDelegate {
                        id: cellRenderer
                        anchors.fill: parent
                        anchors.margins: 6
                        modelData: parent.modelData
                        index: parent.index
                        imageLoader: control.imageLoader
                        customBlockRenderers: control.customBlockRenderers
                        scrollController: control.scrollController
                        imageRevision: control.imageRevision
                        renderEnabled: control.renderEnabled
                        contentColor: control.contentColor
                        mutedColor: control.mutedColor
                        onLinkActivated: link => control.linkActivated(link)
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.color.borderStrong
                    }

                    Rectangle {
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        width: 1
                        color: Theme.color.borderStrong
                    }
                }
            }
        }
    }
}
