import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Lib.Interaction
import Lib.Controls
import Lib.Theme

Popup {
    id: control

    required property string message
    property string confirmText: qsTr("Delete")
    property string cancelText: qsTr("Cancel")
    property var interactionController: null
    property string interactionDomain: "app.dialog"
    property string layerKind: "dialog"
    property var layerToken: 0

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

    function dismissLayer() {
        control.rejected()
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
                false,
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
        InteractionScope.focusable: false
        width: control.width - control.padding * 2
        spacing: 16

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
                interactionController: control.interactionController
                interactionDomain: control.interactionDomain
                interactionGroup: control.interactionDomain
                interactionOrder: 0
                label: control.cancelText
                onClicked: {
                    control.rejected()
                    control.close()
                }
            }

            TextButton {
                interactionController: control.interactionController
                interactionDomain: control.interactionDomain
                interactionGroup: control.interactionDomain
                interactionOrder: 1
                label: control.confirmText
                onClicked: {
                    control.accepted()
                    control.close()
                }
            }
        }
    }
}
