import QtQuick
import Lib.Theme

Column {
    id: control

    property string sourceText: ""
    property bool streaming: false
    property var parser: null
    property bool renderEnabled: true
    property var imageLoader: null
    property var customBlockRenderers: ({})
    property color contentColor: Theme.color.text
    property color mutedColor: Theme.color.textMuted
    property var blocks: []
    property int imageRevision: 0

    spacing: 4

    function containsImageSource(nodes, source) {
        for (let i = 0; i < nodes.length; ++i) {
            const node = nodes[i]
            if (node.kind === "image" && node.destination === source) {
                return true
            }
            if (control.containsImageSource(node.children || [], source)) {
                return true
            }
        }
        return false
    }

    function rebuild() {
        control.blocks = control.parser ? control.parser.parse(control.sourceText) : []
    }

    Component.onCompleted: rebuild()

    onSourceTextChanged: {
        if (control.streaming) {
            parseTimer.restart()
        } else {
            parseTimer.stop()
            control.rebuild()
        }
    }

    onStreamingChanged: {
        if (control.streaming) {
            parseTimer.restart()
        } else {
            parseTimer.stop()
            control.rebuild()
        }
    }

    Timer {
        id: parseTimer
        interval: 80
        repeat: false
        onTriggered: control.rebuild()
    }

    Connections {
        target: control.imageLoader
        ignoreUnknownSignals: true

        function onImageReady(source, localUrl, animated) {
            if (control.containsImageSource(control.blocks, source)) {
                control.imageRevision += 1
            }
        }

        function onImageFailed(source, reason) {
            if (control.containsImageSource(control.blocks, source)) {
                control.imageRevision += 1
            }
        }
    }

    Repeater {
        model: control.blocks

        delegate: MarkdownNodeDelegate {
            width: control.width
            imageLoader: control.imageLoader
            customBlockRenderers: control.customBlockRenderers
            imageRevision: control.imageRevision
            renderEnabled: control.renderEnabled
            contentColor: control.contentColor
            mutedColor: control.mutedColor
            onLinkActivated: link => {
                const lower = link.toLowerCase()
                if (lower.startsWith("http://") || lower.startsWith("https://") || lower.startsWith("mailto:")) {
                    Qt.openUrlExternally(link)
                }
            }
        }
    }
}
