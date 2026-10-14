import QtQuick

Item {
    id: control

    required property var modelData
    required property int index
    readonly property var node: modelData
    readonly property int position: index
    property var imageLoader: null
    property var customBlockRenderers: ({})
    property var scrollController: null
    property int imageRevision: 0
    property string parentKind: ""
    property int listStart: 1
    property string listMarker: "•"
    property int tableColumns: 1
    property var tableColumnWidths: []
    property bool tableTruncate: false
    property int tableMaxLines: 3
    property bool renderEnabled: true
    property color contentColor: "white"
    property color mutedColor: "gray"

    signal linkActivated(string link)

    implicitHeight: loader.item ? loader.item.implicitHeight : 0
    height: implicitHeight

    Loader {
        id: loader
        anchors.left: parent.left
        anchors.right: parent.right
        source: Qt.resolvedUrl("MarkdownNode.qml")

        onLoaded: {
            item.width = control.width
            item.node = control.node
            item.imageLoader = control.imageLoader
            item.customBlockRenderers = control.customBlockRenderers
            item.scrollController = control.scrollController
            item.imageRevision = control.imageRevision
            item.parentKind = control.parentKind
            item.position = control.position
            item.listStart = control.listStart
            item.listMarker = control.listMarker
            item.tableColumns = control.tableColumns
            item.tableColumnWidths = control.tableColumnWidths
            item.tableTruncate = control.tableTruncate
            item.tableMaxLines = control.tableMaxLines
            item.renderEnabled = control.renderEnabled
            item.contentColor = control.contentColor
            item.mutedColor = control.mutedColor
        }
    }

    Connections {
        target: loader.item
        ignoreUnknownSignals: true

        function onLinkActivated(link) {
            control.linkActivated(link)
        }
    }

    onImageRevisionChanged: {
        if (loader.item) {
            loader.item.imageRevision = control.imageRevision
        }
    }

    onTableColumnWidthsChanged: {
        if (loader.item) {
            loader.item.tableColumnWidths = control.tableColumnWidths
        }
    }

    onTableTruncateChanged: {
        if (loader.item) {
            loader.item.tableTruncate = control.tableTruncate
        }
    }

    onTableMaxLinesChanged: {
        if (loader.item) {
            loader.item.tableMaxLines = control.tableMaxLines
        }
    }
}
