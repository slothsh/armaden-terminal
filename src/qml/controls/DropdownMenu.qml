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
    property string actionSurface: "menu"
    property var actionTarget: 0
    property var allowedActionIds: []
    property var actionModel: []
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

    onActionTargetChanged: control.refreshActions()
    onAllowedActionIdsChanged: control.refreshActions()
    onActionSurfaceChanged: control.refreshActions()
    onInteractionControllerChanged: control.refreshActions()

    function refreshActions() {
        if (!control.interactionController || control.actionTarget === 0) {
            control.actionModel = []
            return
        }

        const revision = control.interactionController.actionRevision
        const actions = revision >= 0
            ? control.interactionController.actionsFor(control.actionTarget, control.actionSurface)
            : []
        control.actionModel = control.allowedActionIds.length > 0
            ? actions.filter(action => control.allowedActionIds.indexOf(action.id) >= 0)
            : actions
    }

    onAboutToShow: {
        if (control.interactionController && control.actionTarget === 0) {
            control.actionTarget = control.interactionController.targetForDomain(control.interactionDomain, "layer-content")
        }
        control.refreshActions()
        if (control.interactionController && control.layerToken === 0) {
            control.layerToken = control.interactionController.pushLayer(
                control,
                control.layerKind,
                control.interactionDomain,
                control.actionTarget,
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
        InteractionScope.focusable: false
        InteractionScope.activatable: true
        spacing: 0

        Repeater {
            model: control.actionModel

            delegate: Column {
                required property var modelData
                required property int index

                width: control.width - control.padding * 2
                spacing: 0

                Rectangle {
                    readonly property bool separatorVisible: index > 0
                        && control.actionModel[index - 1].group !== modelData.group
                    width: parent.width
                    height: separatorVisible ? 9 : 0
                    color: "transparent"
                    visible: separatorVisible

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 1
                        color: "#1affffff"
                    }
                }

                MenuButton {
                    width: parent.width
                    label: modelData.label
                    interactionDomain: control.interactionDomain
                    interactionGroup: control.interactionDomain
                    interactionOrder: modelData.order
                    actionId: modelData.id
                    actionSurface: control.actionSurface
                    actionTarget: control.actionTarget
                    interactionController: control.interactionController
                    onClicked: {
                        if (control.interactionController.invokeAction(modelData.id, control.actionTarget)) {
                            control.close()
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: control.interactionController

        function onActionsChanged() {
            control.refreshActions()
        }
    }
}
