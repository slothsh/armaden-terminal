import QtQuick
import QtQuick.Controls
import Lib.Controls
import Lib.Theme

Rectangle {
    id: control

    property alias text: area.text
    property alias placeholderText: area.placeholderText
    property bool readOnly: false

    signal accepted()

    color: Theme.color.menu

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
            width: scroll.width
            height: contentHeight + topPadding + bottomPadding
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

            onCursorRectangleChanged: control.ensureCursorVisible()

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Return && event.modifiers & Qt.ControlModifier) {
                    control.accepted()
                    event.accepted = true
                }
            }
        }
    }
}
