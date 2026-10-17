import QtQuick
import QtQuick.Window
import Lib.Theme

Window {
    id: root

    property var chat: null
    property var interaction: null
    property var markdownImages: null

    signal closeInterceptRequested(var close)

    visible: true
    color: Theme.color.dark
    width: 960
    height: 640
    title: qsTr("mindtosis")

    onClosing: close => root.closeInterceptRequested(close)
}
