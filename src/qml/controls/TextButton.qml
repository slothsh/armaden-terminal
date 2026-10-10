import QtQuick
import Lib.Interaction
import Lib.Theme

Rectangle {
    id: control

    InteractionScope.domain: control.interactionDomain
    InteractionScope.role: control.interactionRole
    InteractionScope.label: control.label
    InteractionScope.group: control.interactionGroup
    InteractionScope.order: control.interactionOrder
    InteractionScope.focusable: true
    InteractionScope.activatable: true
    InteractionScope.navigable: true

    required property string label
    property string interactionDomain: "app.controls"
    property string interactionRole: "button"
    property string interactionGroup: ""
    property int interactionOrder: 0
    property string actionId: ""
    property string actionSurface: "button"
    property var interactionController: null
    property var actionTarget: 0
    property alias hovered: mouseArea.containsMouse
    readonly property bool actionAllowed: {
        if (control.actionId.length === 0) {
            return true
        }
        if (!control.interactionController) {
            return false
        }

        return control.interactionController.actionRevision >= 0
            && control.interactionController.actionEnabled(control.actionId, control.actionTarget, control.actionSurface)
    }

    signal clicked()

    function activate() {
        if (control.enabled && control.actionAllowed) {
            control.clicked()
        }
    }

    enabled: control.actionAllowed
    implicitWidth: labelText.implicitWidth + 24
    implicitHeight: 30
    radius: Theme.metric.radius
    opacity: control.enabled && control.actionAllowed ? 1.0 : 0.45
    color: control.enabled && control.actionAllowed && (control.hovered || control.activeFocus) ? Theme.color.hover : "transparent"

    Text {
        id: labelText
        anchors.centerIn: parent
        text: control.label
        color: Theme.color.text
        font.pixelSize: 13
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: control.enabled && control.actionAllowed
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        onClicked: control.clicked()
    }
}
