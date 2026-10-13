import QtQuick
import Lib.Controls
import Lib.Interaction
import Lib.Theme

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
    property real maxCodeHeight: 360
    property real wheelStep: 90
    readonly property bool codeOverflows: codeViewport.contentHeight > codeViewport.height + 1
        || codeViewport.contentWidth > codeViewport.width + 1
    readonly property bool scrollEnabled: control.codeOverflows && (control.activeFocus || codeText.activeFocus)

    signal interactionScrollRequested(real delta)

    onInteractionScrollRequested: delta => {
        if (!control.scrollEnabled) {
            return
        }

        const maxContentY = Math.max(0, codeViewport.contentHeight - codeViewport.height)
        codeViewport.contentY = Math.max(0, Math.min(
            maxContentY,
            codeViewport.contentY - delta * control.wheelStep
        ))
    }

    radius: Theme.metric.radius
    color: Theme.color.dark
    border.color: Theme.color.borderStrong
    border.width: 1
    implicitHeight: header.height + Math.min(control.maxCodeHeight, Math.max(40, codeText.contentHeight + 16))
    clip: true

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 30
        color: Theme.color.mediumBright

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.metric.spacing
            anchors.verticalCenter: parent.verticalCenter
            text: control.language.length > 0 ? control.language : qsTr("Code")
            color: Theme.color.textMuted
            font.pixelSize: 11
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
            anchors.rightMargin: 4
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

    Flickable {
        id: codeViewport
        objectName: "codeViewport"
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        clip: true
        interactive: control.scrollEnabled
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: control.wordWrap ? width : Math.max(width, codeText.contentWidth)
        contentHeight: Math.max(height, codeText.contentHeight)

        TextEdit {
            id: codeText
            objectName: "markdownCodeText"
            width: control.wordWrap ? codeViewport.width : Math.max(codeViewport.width, contentWidth)
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
}
