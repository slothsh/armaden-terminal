import QtQuick
import QtQuick.Controls
import Lib.Theme

Popup {
    id: control

    required property string title
    property string version: ""
    property string description: ""

    modal: true
    width: 320
    padding: 16
    implicitHeight: contentColumn.implicitHeight + padding * 2
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2

    background: Rectangle {
        color: Theme.color.menu
        border.color: Theme.color.border
        border.width: 1
        radius: Theme.metric.radius
    }

    contentItem: Column {
        id: contentColumn
        width: control.width - control.padding * 2
        spacing: 8

        Text {
            text: control.title
            color: Theme.color.text
            font.pixelSize: 16
            font.bold: true
        }

        Text {
            text: control.version
            color: Theme.color.textMuted
            font.pixelSize: 12
        }

        Text {
            width: contentColumn.width
            text: control.description
            color: Theme.color.text
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
