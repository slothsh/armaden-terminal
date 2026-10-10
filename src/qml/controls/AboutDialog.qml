import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Lib.Interaction
import Lib.Theme

Popup {
    id: control

    required property string title
    property string version: ""
    property string description: ""
    property var interactionController: null
    property string interactionDomain: "app.dialog"
    property string layerKind: "dialog"
    property var layerToken: 0

    modal: true
    width: 320
    padding: 16
    focus: true
    implicitHeight: contentColumn.implicitHeight + padding * 2
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    closePolicy: Popup.CloseOnPressOutside

    function dismissLayer() {
        control.close()
    }

    onAboutToShow: {
        if (control.interactionController && control.layerToken === 0) {
            control.layerToken = control.interactionController.pushLayer(
                control,
                control.layerKind,
                control.interactionDomain,
                0,
                true,
                true,
                true
            )
        }
    }

    onClosed: {
        if (control.interactionController && control.layerToken !== 0) {
            control.interactionController.popLayer(control.layerToken)
            control.layerToken = 0
        }
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
        InteractionScope.domain: control.interactionDomain
        InteractionScope.role: "layer-content"
        InteractionScope.focusable: true
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
