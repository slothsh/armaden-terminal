import QtQuick
import Lib.Theme

Rectangle {
    id: control

    required property string label
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

    implicitHeight: 26
    implicitWidth: 160
    radius: 2
    opacity: control.enabled && control.actionAllowed ? 1.0 : 0.45
    color: control.enabled && control.actionAllowed && control.hovered ? Theme.color.hover : "transparent"

    Text {
        anchors.verticalCenter: parent.verticalCenter
        x: 8
        text: control.label
        color: Theme.color.text
        font.pixelSize: 12
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
