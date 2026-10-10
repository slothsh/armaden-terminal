import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Lib.Interaction
import Lib.Theme

Popup {
    id: control

    property var interactionController: null
    property string interactionDomain: "app.popup"
    property string layerKind: "popup"
    property bool dismissOnEscape: true
    property bool dismissOnOutsideClick: true
    property bool focusTrap: true
    property var layerToken: 0

    width: 160
    padding: 4
    focus: true
    implicitHeight: contentColumn.implicitHeight + padding * 2
    closePolicy: Popup.CloseOnPressOutside

    function toggle() {
        if (control.interactionController
                && control.interactionController.consumeOutsideDismissal(control)) {
            return
        }
        if (control.opened) {
            control.close()
        } else {
            control.open()
        }
    }

    function dismissLayer() {
        control.close()
    }

    function dismissLayerOutside() {
        control.close()
    }

    onAboutToShow: {
        if (control.interactionController && control.layerToken === 0) {
            control.layerToken = control.interactionController.pushLayer(
                control,
                control.layerKind,
                control.interactionDomain,
                0,
                control.dismissOnEscape,
                control.dismissOnOutsideClick,
                control.focusTrap
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
        spacing: 0
    }
}
