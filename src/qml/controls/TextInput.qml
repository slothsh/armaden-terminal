import QtQuick
import QtQuick.Controls
import Lib.Theme

Rectangle {
    id: control

    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property bool readOnly: false

    signal accepted()

    color: Theme.color.menu

    ScrollView {
        id: scroll
        anchors.fill: parent
        clip: true
        background: null

        TextArea {
            id: area
            width: scroll.availableWidth
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

            cursorDelegate: Rectangle {
                width: 2
                color: Theme.color.accent
            }

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Return && event.modifiers & Qt.ControlModifier) {
                    control.accepted()
                    event.accepted = true
                }
            }
        }
    }
}
