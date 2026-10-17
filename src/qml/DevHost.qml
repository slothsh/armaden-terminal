import QtQuick

import Application

AppWindow {
    id: root

    property var hotReload: null

    readonly property bool viewReady: viewLoader.status === Loader.Ready

    MarkdownParser {
        id: contentParser
    }

    Shortcut {
        sequence: "F5"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (root.hotReload !== null) {
                root.hotReload.reload()
            }
        }
    }

    Loader {
        id: viewLoader
        anchors.fill: parent
    }

    Rectangle {
        id: errorOverlay
        anchors.fill: parent
        z: 1000
        visible: root.hotReload !== null && root.hotReload.lastError.length > 0
        color: "#d0000000"

        Text {
            anchors.fill: parent
            anchors.margins: 16
            text: root.hotReload !== null ? root.hotReload.lastError : ""
            color: "#ff6b6b"
            font.family: "monospace"
            font.pointSize: 9
            wrapMode: Text.WrapAnywhere
            elide: Text.ElideNone
        }
    }

    Component.onCompleted: loadView()

    function loadView() {
        const component = Qt.createComponent(root.hotReload.mainSource)
        if (component.status === Component.Error) {
            root.hotReload.reportError(component.errorString())
            return
        }
        viewLoader.setSource(root.hotReload.mainSource, {
            chat: root.chat,
            interaction: root.interaction,
            markdownImages: root.markdownImages,
            markdownParser: contentParser,
        })
    }
}
