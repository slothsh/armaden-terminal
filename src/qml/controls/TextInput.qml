import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import Lib.Interaction
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    InteractionScope.domain: control.interactionDomain
    InteractionScope.role: control.interactionRole
    InteractionScope.label: control.placeholderText
    InteractionScope.focusable: true
    InteractionScope.activatable: true
    InteractionScope.contextMenuable: true
    InteractionScope.editable: true
    InteractionScope.navigable: true
    InteractionScope.group: control.interactionGroup
    InteractionScope.order: control.interactionOrder

    property string interactionDomain: "app.prompt"
    property string interactionRole: "input"
    property string interactionGroup: ""
    property int interactionOrder: 0
    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property bool readOnly: false

    signal accepted()
    signal contextMenuRequested(real x, real y)

    color: Theme.color.menu

    function activate() {
        control.accepted()
    }

    function handleKeyPress(key, modifiers) {
        const isEnter = key === Qt.Key_Return || key === Qt.Key_Enter
        if (isEnter && (modifiers & Qt.ControlModifier)) {
            control.accepted()
            return true
        }

        return false
    }

    function ensureCursorVisible() {
        const r = area.cursorRectangle;
        const top = r.y - area.topPadding;
        const bottom = r.y + r.height + area.bottomPadding;
        if (bottom > scroll.contentY + scroll.height) {
            scroll.contentY = Math.min(bottom - scroll.height, scroll.maxContentY());
        } else if (top < scroll.contentY) {
            scroll.contentY = Math.max(top, 0);
        }
    }

    ScrollingArea {
        id: scroll
        anchors.fill: parent
        barTopMargin: 8
        barBottomMargin: 8

        TextArea {
            id: area
            objectName: "promptTextArea"
            T.ContextMenu.menu: null
            T.ContextMenu.onRequested: function(position) {
                control.contextMenuRequested(position.x, position.y)
            }
            width: scroll.width
            height: Math.max(scroll.viewportHeight, contentHeight + topPadding + bottomPadding)
            leftPadding: 10
            rightPadding: 10
            topPadding: 8
            bottomPadding: 8
            color: Theme.color.text
            placeholderTextColor: Theme.color.textMuted
            selectionColor: Theme.color.accent
            selectedTextColor: Theme.color.text
            font.pixelSize: 13
            wrapMode: TextArea.Wrap
            background: null
            readOnly: control.readOnly

            Keys.priority: Keys.BeforeItem
            Keys.onPressed: event => {
                if (control.handleKeyPress(event.key, event.modifiers)) {
                    event.accepted = true
                }
            }

            cursorDelegate: Rectangle {
                width: 2
                color: Theme.color.accent
            }

            onCursorRectangleChanged: control.ensureCursorVisible()

        }
    }
}
