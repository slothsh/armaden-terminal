import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Lib.Controls
import Lib.Theme

Popup {
    id: control

    required property string message
    property string confirmText: qsTr("Delete")
    property string cancelText: qsTr("Cancel")

    signal accepted()
    signal rejected()

    modal: true
    width: 360
    padding: 16
    focus: true
    implicitHeight: contentColumn.implicitHeight + padding * 2
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.NoAutoClose

    background: Item {
        RectangularShadow {
            anchors.fill: parent
            radius: Theme.metric.radius
            color: "#40000000"
            blur: 16
            spread: 0
            offset: Qt.point(0, 4)
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.color.popup
            border.color: Theme.color.borderStrong
            border.width: 1
            radius: Theme.metric.radius
        }
    }

    contentItem: Column {
        id: contentColumn
        width: control.width - control.padding * 2
        spacing: 16

        Keys.onEscapePressed: {
            control.rejected()
            control.close()
            event.accepted = true
        }

        Shortcut {
            sequence: "Escape"
            context: Qt.WindowShortcut
            enabled: control.opened
            onActivated: {
                control.rejected()
                control.close()
            }
        }

        Text {
            width: parent.width
            text: control.message
            color: Theme.color.text
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }

        Row {
            anchors.right: parent.right
            spacing: 8

            TextButton {
                label: control.cancelText
                onClicked: {
                    control.rejected()
                    control.close()
                }
            }

            TextButton {
                label: control.confirmText
                onClicked: {
                    control.accepted()
                    control.close()
                }
            }
        }
    }
}
