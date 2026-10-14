import QtQuick
import QtQuick.Layouts
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
    property real tableMaxColumnWidth: 360
    property real tableMinColumnWidth: 64
    property var tableColumnWidths: []
    property bool tableTruncate: false
    property int tableMaxLines: 3
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

    function plainTextOf(item) {
        const children = item.children || []
        switch (item.kind) {
            case "text": return item.text || ""
            case "entity": return item.text || ""
            case "hardBreak": return "\n"
            case "softBreak": return " "
            case "image": return "[" + (item.text || "") + "]"
            case "rawHtml": return item.text || ""
            default: break
        }
        return children.length > 0 ? control.inlinePlainText(children) : (item.text || "")
    }

    function inlinePlainText(nodes) {
        let result = ""
        for (let index = 0; index < nodes.length; ++index) {
            result += control.plainTextOf(nodes[index])
        }
        return result
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

    function tableRows(item) {
        const rows = []
        const groups = item.children || []
        for (let groupIndex = 0; groupIndex < groups.length; ++groupIndex) {
            const group = groups[groupIndex]
            const isHeader = group.kind === "tableHead"
            const groupRows = group.children || []
            for (let rowIndex = 0; rowIndex < groupRows.length; ++rowIndex) {
                const row = groupRows[rowIndex]
                rows.push({
                    kind: row.kind,
                    children: row.children || [],
                    tableGroup: isHeader ? "tableHead" : "tableBody",
                    tableFirstRow: false,
                    tableLastRow: false
                })
            }
        }
        if (rows.length > 0) {
            rows[0].tableFirstRow = true
            rows[rows.length - 1].tableLastRow = true
        }
        return rows
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
        if (node.kind === "table") {
            return tableComponent
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
            implicitHeight: control.tableTruncate
                ? Math.max(truncatedText.implicitHeight, truncatedText.font.pixelSize * 1.4)
                : Math.max(richText.contentHeight, richText.font.pixelSize * 1.4)
            height: implicitHeight

            TextEdit {
                id: richText
                objectName: "markdownRichText"
                visible: !control.tableTruncate
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

            Text {
                id: truncatedText
                objectName: "markdownTruncatedText"
                visible: control.tableTruncate
                anchors.fill: parent
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: Math.max(1, control.tableMaxLines)
                text: control.inlinePlainText(control.node.children || [])
                color: control.contentColor
                font.pixelSize: 12
                horizontalAlignment: control.node.alignment === 2
                    ? Text.AlignHCenter
                    : (control.node.alignment === 3 ? Text.AlignRight : Text.AlignLeft)
            }

            MouseArea {
                objectName: "markdownRichTextWheelCapture"
                visible: !control.tableTruncate
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
        id: tableComponent

        Item {
            width: control.width
            implicitHeight: tableFrame.implicitHeight
            height: implicitHeight

            Rectangle {
                id: tableFrame
                objectName: "tableFrame"

                readonly property int cellPadding: 6
                readonly property int columnCount: control.tableColumnCount(control.node)
                readonly property real maxColumnWidth: control.node.maxColumnWidth > 0
                    ? control.node.maxColumnWidth
                    : control.tableMaxColumnWidth
                readonly property real minColumnWidth: control.node.minColumnWidth > 0
                    ? control.node.minColumnWidth
                    : control.tableMinColumnWidth
                readonly property bool truncate: control.node.truncate === true
                readonly property int maxLines: control.node.maxLines > 0
                    ? control.node.maxLines
                    : control.tableMaxLines
                readonly property var columnWidths: {
                    const revision = tableMeasure.revision
                    const available = Math.max(0, control.width - 2)
                    const widths = []
                    let total = 0
                    for (let columnIndex = 0; columnIndex < columnCount; ++columnIndex) {
                        const natural = naturalColumnWidth(columnIndex) + 2 * cellPadding
                        const width = Math.max(minColumnWidth, Math.min(maxColumnWidth, natural))
                        widths.push(width)
                        total += width
                    }
                    if (total > available && total > 0) {
                        const factor = available / total
                        for (let columnIndex = 0; columnIndex < widths.length; ++columnIndex) {
                            widths[columnIndex] = Math.max(minColumnWidth, Math.floor(widths[columnIndex] * factor))
                        }
                    }
                    return widths
                }
                readonly property real totalColumnWidth: {
                    const widths = tableFrame.columnWidths
                    let total = 0
                    for (let columnIndex = 0; columnIndex < widths.length; ++columnIndex) {
                        total += widths[columnIndex]
                    }
                    return total
                }

                function naturalColumnWidth(columnIndex) {
                    let widest = 0
                    const measureRows = tableMeasure.children
                    for (let rowIndex = 0; rowIndex < measureRows.length; ++rowIndex) {
                        const cells = measureRows[rowIndex].children
                        if (cells === undefined) {
                            continue
                        }
                        for (let cellIndex = 0; cellIndex < cells.length; ++cellIndex) {
                            const cell = cells[cellIndex]
                            if (cell.measureColumn === columnIndex) {
                                widest = Math.max(widest, cell.implicitWidth)
                            }
                        }
                    }
                    return widest
                }

                width: totalColumnWidth > 0 ? Math.min(control.width, Math.ceil(totalColumnWidth + 2)) : control.width
                implicitHeight: tableColumn.implicitHeight + 2
                height: implicitHeight
                radius: Theme.metric.radius
                color: Theme.color.dark

                Rectangle {
                    id: tableClip
                    anchors.fill: parent
                    anchors.margins: 1
                    color: "transparent"
                    clip: true

                    Column {
                        id: tableColumn
                        anchors.fill: parent

                        Repeater {
                            model: control.tableRows(control.node)

                            delegate: MarkdownNodeDelegate {
                                width: tableColumn.width
                                imageLoader: control.imageLoader
                                customBlockRenderers: control.customBlockRenderers
                                scrollController: control.scrollController
                                imageRevision: control.imageRevision
                                renderEnabled: control.renderEnabled
                                tableColumnWidths: tableFrame.columnWidths
                                tableTruncate: tableFrame.truncate
                                tableMaxLines: tableFrame.maxLines
                                contentColor: control.contentColor
                                mutedColor: control.mutedColor
                                onLinkActivated: link => control.linkActivated(link)
                            }
                        }
                    }
                }

                Rectangle {
                    id: tableBorder
                    anchors.fill: parent
                    radius: tableFrame.radius
                    color: "transparent"
                    border.color: Theme.color.borderStrong
                    border.width: 1
                }

                Item {
                    id: tableMeasure
                    visible: false

                    property int revision: 0

                    Repeater {
                        model: control.tableRows(control.node)
                        onCountChanged: tableMeasure.revision++

                        delegate: Row {
                            required property var modelData

                            Repeater {
                                model: modelData.children
                                onCountChanged: tableMeasure.revision++

                                delegate: Text {
                                    required property var modelData
                                    required property int index

                                    readonly property int measureColumn: index

                                    textFormat: Text.RichText
                                    wrapMode: Text.NoWrap
                                    text: control.richTextHtml(modelData)
                                    font.pixelSize: 12
                                    onImplicitWidthChanged: tableMeasure.revision++
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: tableRowComponent

        RowLayout {
            id: row
            objectName: "tableRow"
            width: control.width
            spacing: 0

            Repeater {
                model: control.node.children || []

                delegate: Rectangle {
                    id: cell
                    objectName: "tableCell"

                    required property var modelData
                    required property int index

                    readonly property bool firstColumn: index === 0
                    readonly property bool lastColumn: index === control.node.children.length - 1
                    readonly property real cellRadius: Math.max(0, Theme.metric.radius - 1)
                    readonly property real columnWidth: (control.tableColumnWidths
                        && index < control.tableColumnWidths.length)
                        ? control.tableColumnWidths[index]
                        : row.width / Math.max(1, control.node.children.length)

                    Layout.fillHeight: true
                    Layout.preferredWidth: cell.columnWidth
                    implicitHeight: cellRenderer.implicitHeight + 12
                    color: control.node.tableGroup === "tableHead" ? Theme.color.mediumBright : Theme.color.dark
                    topLeftRadius: control.node.tableFirstRow && firstColumn ? cellRadius : 0
                    topRightRadius: control.node.tableFirstRow && lastColumn ? cellRadius : 0
                    bottomLeftRadius: control.node.tableLastRow && firstColumn ? cellRadius : 0
                    bottomRightRadius: control.node.tableLastRow && lastColumn ? cellRadius : 0

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
                        tableTruncate: control.tableTruncate
                        tableMaxLines: control.tableMaxLines
                        contentColor: control.contentColor
                        mutedColor: control.mutedColor
                        onLinkActivated: link => control.linkActivated(link)
                    }

                    Rectangle {
                        visible: !control.node.tableLastRow
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.color.borderStrong
                    }

                    Rectangle {
                        visible: index < control.node.children.length - 1
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
