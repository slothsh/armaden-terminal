import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Lib.Theme

Popup {
    id: control

    property bool justClosed: false

    width: 160
    padding: 4
    focus: true
    implicitHeight: contentColumn.implicitHeight + padding * 2

    function toggle() {
        if (control.opened) {
            control.close()
            return
        }
        if (control.justClosed) {
            control.justClosed = false
            return
        }
        control.open()
    }

    onClosed: {
        control.justClosed = true
        guard.restart()
    }

    Timer {
        id: guard
        interval: 150
        onTriggered: control.justClosed = false
    }

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
        spacing: 0

        Keys.onEscapePressed: {
            control.close()
            event.accepted = true
        }

        Shortcut {
            sequence: "Escape"
            context: Qt.WindowShortcut
            enabled: control.opened
            onActivated: control.close()
        }
    }
}
