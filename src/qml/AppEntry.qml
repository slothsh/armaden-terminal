import QtQuick

import Application

AppWindow {
    id: root

    MarkdownParser {
        id: contentParser
    }

    Main {
        id: mainView
        anchors.fill: parent
        chat: root.chat
        interaction: root.interaction
        markdownImages: root.markdownImages
        markdownParser: contentParser
    }

    onCloseInterceptRequested: close => mainView.interceptClose(close)
}
