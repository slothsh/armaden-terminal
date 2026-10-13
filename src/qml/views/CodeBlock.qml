import QtQuick
import Lib.Controls
import Lib.Interaction
import Lib.Theme
import Lucide

Rectangle {
    id: control

    InteractionScope.domain: "app.conversation"
    InteractionScope.role: "code-block"
    InteractionScope.focusable: true
    InteractionScope.scrollable: control.scrollEnabled
    InteractionScope.hitTestable: true

    required property string code
    property string language: ""
    property bool wordWrap: true
    property bool expanded: false
    property real collapsedHeight: 360
    property real wheelStep: 90
    readonly property bool codeOverflows: codeViewport.contentHeight > codeViewport.viewportHeight + 1
        || codeViewport.contentWidth > codeViewport.viewportWidth + 1
    readonly property bool scrollEnabled: control.codeOverflows && (control.activeFocus || codeText.activeFocus)
    readonly property bool hasHiddenContent: codeViewport.contentHeight > codeViewport.viewportHeight + 1
    readonly property bool expandToggleVisible: control.expanded || control.hasHiddenContent

    signal interactionScrollRequested(real delta)

    onInteractionScrollRequested: delta => {
        if (!control.scrollEnabled) {
            return
        }

        codeViewport.interactionScrollRequested(delta)
    }

    radius: Theme.metric.radius
    color: Theme.color.dark
    border.color: Theme.color.borderStrong
    border.width: 1
    implicitHeight: header.height + (control.expanded
        ? Math.max(40, codeText.contentHeight + 16)
        : Math.min(control.collapsedHeight, Math.max(40, codeText.contentHeight + 16)))
    clip: true

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 38
        color: Theme.color.mediumBright

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.metric.spacing
            anchors.verticalCenter: parent.verticalCenter
            text: control.language.length > 0 ? control.language : qsTr("Code")
            color: Theme.color.textMuted
            font.pixelSize: 11
        }

        TextButton {
            id: expandToggleButton
            objectName: "codeExpandToggleButton"
            anchors.right: wrapButton.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            visible: control.expandToggleVisible
            label: control.expanded ? qsTr("Collapse") : qsTr("Expand")
            onClicked: control.expanded = !control.expanded
        }

        IconButton {
            id: wrapButton
            objectName: "codeWrapButton"
            anchors.right: copyButton.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            iconName: control.wordWrap ? "move-horizontal" : "wrap-text"
            accessibleLabel: control.wordWrap ? qsTr("Disable code wrapping") : qsTr("Enable code wrapping")
            onClicked: control.wordWrap = !control.wordWrap

            HoverHandler {
                objectName: "codeWrapPointerCursor"
                cursorShape: Qt.PointingHandCursor
            }
        }

        IconButton {
            id: copyButton
            objectName: "codeCopyButton"
            anchors.right: parent.right
            anchors.rightMargin: Theme.metric.spacing
            anchors.verticalCenter: parent.verticalCenter
            iconName: "copy"
            accessibleLabel: qsTr("Copy code")
            onClicked: {
                const cursor = codeText.cursorPosition
                codeText.selectAll()
                codeText.copy()
                codeText.deselect()
                codeText.cursorPosition = cursor
            }

            HoverHandler {
                objectName: "codeCopyPointerCursor"
                cursorShape: Qt.PointingHandCursor
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.color.borderStrong
        }
    }

    ScrollingArea {
        id: codeViewport
        objectName: "codeViewport"
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        interactionEnabled: false
        horizontalScrollEnabled: !control.wordWrap
        contentWidthHint: control.wordWrap ? -1 : codeText.contentWidth
        wheelStep: control.wheelStep

        TextEdit {
            id: codeText
            objectName: "markdownCodeText"
            width: control.wordWrap ? codeViewport.viewportWidth : Math.max(codeViewport.viewportWidth, contentWidth)
            height: Math.max(contentHeight, font.pixelSize * 1.4)
            readOnly: true
            selectByMouse: true
            textFormat: TextEdit.PlainText
            wrapMode: control.wordWrap ? TextEdit.Wrap : TextEdit.NoWrap
            text: control.code
            color: Theme.color.text
            font.family: "monospace"
            font.pixelSize: 12
            selectionColor: Theme.color.accent
            selectedTextColor: Theme.color.text

            HoverHandler {
                objectName: "codeTextCursor"
                cursorShape: Qt.IBeamCursor
            }
        }
    }

    Item {
        id: expandButton
        objectName: "codeExpandButton"
        width: 24
        height: 24
        visible: control.expandToggleVisible
        anchors.horizontalCenter: codeViewport.horizontalCenter
        anchors.bottom: codeViewport.bottom
        anchors.bottomMargin: control.wordWrap ? 2 : 12
        z: 20

        LucideIcon {
            objectName: "codeExpandIcon"
            anchors.centerIn: parent
            name: control.expanded ? "arrow-up" : "arrow-down"
            size: 18
            strokeWidth: 2
            color: expandMouse.containsMouse ? Theme.color.text : Theme.color.textMuted
        }

        MouseArea {
            id: expandMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.expanded = !control.expanded
        }
    }
}
