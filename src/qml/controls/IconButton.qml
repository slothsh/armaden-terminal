import QtQuick
import Lib.Interaction
import Lib.Theme
import Lucide

Rectangle {
    id: control

    InteractionScope.domain: control.interactionDomain
    InteractionScope.role: control.interactionRole
    InteractionScope.label: control.accessibleLabel.length > 0 ? control.accessibleLabel : control.iconName
    InteractionScope.group: control.interactionGroup
    InteractionScope.order: control.interactionOrder
    InteractionScope.focusable: true
    InteractionScope.activatable: true
    InteractionScope.navigable: true

    required property string iconName
    property string accessibleLabel: ""
    property string interactionDomain: "app.controls"
    property string interactionRole: "button"
    property string interactionGroup: ""
    property int interactionOrder: 0
    property real iconSize: 16
    property color iconColor: Theme.color.text
    property bool active: false
    property bool interactive: true
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
        if (control.enabled && control.actionAllowed && control.interactive) {
            control.clicked()
        }
    }

    enabled: control.actionAllowed && control.interactive
    implicitWidth: 24
    implicitHeight: 24
    radius: Theme.metric.radius
    opacity: control.interactive && control.enabled && control.actionAllowed ? 1.0 : 0.4
    color: control.interactive && control.enabled && control.actionAllowed && control.hovered
        ? Theme.color.hover
        : (control.activeFocus || (control.interactive && control.active) ? Theme.color.active : "transparent")

    LucideIcon {
        anchors.centerIn: parent
        name: control.iconName
        size: control.iconSize
        color: control.iconColor
        strokeWidth: 2
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: control.enabled && control.interactive && control.actionAllowed
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        onClicked: {
            if (control.interactive && control.enabled && control.actionAllowed) {
                control.clicked()
            }
        }
    }
}
