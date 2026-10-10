import QtQuick
import QtQuick.Window
import Lib.Interaction
import Lib.Theme

Item {
    id: control

    InteractionScope.domain: control.interactionDomain
    InteractionScope.role: "scroll-area"
    InteractionScope.scrollable: true
    InteractionScope.hitTestable: true

    property string interactionDomain: "app.scroll"

    default property alias content: contentColumn.data

    property bool stickToBottom: false
    property int bottomPadding: 0
    property int barTopMargin: 0
    property int barBottomMargin: 0
    property int wheelStep: 90
    property color barColor: "#c8c8c8"
    property int barWidth: 6
    property int barRightMargin: 2
    property int barMinHeight: 24
    property int gutterWidth: 16
    property int hideDelay: 2000
    property int hideDuration: 150
    property int contentCursorShape: Qt.ArrowCursor
    property int contentCursorStart: 0
    property int contentCursorWidth: -1

    property alias contentY: flick.contentY
    property alias contentHeight: flick.contentHeight
    property alias viewportHeight: flick.height

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: contentColumn.height + control.bottomPadding

        onContentHeightChanged: {
            if (control.stickToBottom) {
                control.snapToEnd();
            }
        }

        Column {
            id: contentColumn
            width: flick.width
            spacing: 0
        }
    }

    function maxContentY() {
        return flick.originY + Math.max(0, flick.contentHeight - flick.height);
    }

    function atBottom() {
        return flick.contentY >= control.maxContentY() - 1;
    }

    function snapToEnd() {
        flick.contentY = control.maxContentY();
        Qt.callLater(function() {
            flick.contentY = control.maxContentY();
        });
    }

    function cursorShapeAt(x, y) {
        if (control.contentCursorWidth < 0)
            return control.contentCursorShape;
        const insideX = x >= control.contentCursorStart
            && x < control.contentCursorStart + control.contentCursorWidth;
        const contentBottom = contentColumn.height + flick.originY - flick.contentY;
        return (insideX && y < contentBottom) ? control.contentCursorShape : Qt.ArrowCursor;
    }

    function scrollBy(delta) {
        flick.contentY = Math.max(flick.originY, Math.min(flick.contentY + delta, control.maxContentY()));
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        hoverEnabled: true
        cursorShape: control.cursorShapeAt(mouseX, mouseY)

        onWheel: wheel => {
            const delta = wheel.pixelDelta.y !== 0
                ? wheel.pixelDelta.y
                : wheel.angleDelta.y / 120 * control.wheelStep;
            control.scrollBy(-delta);
            scrollBar.poke();
            wheel.accepted = true;
        }
    }

    Rectangle {
        id: scrollBar
        anchors.right: parent.right
        anchors.rightMargin: control.barRightMargin
        width: control.barWidth
        radius: width / 2
        color: control.barColor
        z: 10
        visible: flick.contentHeight > flick.height
        readonly property bool revealed: Window.active && (gutterHover.hovered || dragArea.pressed)
        opacity: 0
        readonly property real trackTop: control.barTopMargin
        readonly property real trackHeight: Math.max(0, control.height - control.barTopMargin - control.barBottomMargin)
        readonly property real contentRange: Math.max(0, flick.contentHeight - flick.height)
        height: Math.min(trackHeight, Math.max(control.barMinHeight, trackHeight * flick.height / Math.max(1, flick.contentHeight)))
        y: trackTop + (contentRange <= 0
            ? 0
            : (flick.contentY - flick.originY) / contentRange * (trackHeight - height))

        onRevealedChanged: {
            if (revealed) {
                hideTimer.stop()
                hideAnimation.stop()
                scrollBar.opacity = 1
            } else {
                hideTimer.restart()
            }
        }

        function poke() {
            hideTimer.stop()
            hideAnimation.stop()
            scrollBar.opacity = 1
            hideTimer.restart()
        }

        Timer {
            id: hideTimer
            interval: control.hideDelay
            onTriggered: {
                if (!scrollBar.revealed) {
                    hideAnimation.restart()
                }
            }
        }

        NumberAnimation {
            id: hideAnimation
            target: scrollBar
            property: "opacity"
            to: 0
            duration: control.hideDuration
            easing.type: Easing.Linear
        }

        MouseArea {
            id: dragArea
            anchors.fill: parent
            enabled: scrollBar.opacity > 0
            cursorShape: Qt.PointingHandCursor

            property real grab: 0

            onPressed: mouse => {
                dragArea.grab = mapToItem(flick, mouse.x, mouse.y).y - (scrollBar.y - flick.y);
            }

            onPositionChanged: mouse => {
                if (!dragArea.pressed) {
                    return;
                }

                const travel = Math.max(1, scrollBar.trackHeight - scrollBar.height);
                const top = mapToItem(flick, mouse.x, mouse.y).y - dragArea.grab;
                const ratio = Math.max(0, Math.min(1, (top - scrollBar.trackTop) / travel));
                flick.contentY = flick.originY + ratio * scrollBar.contentRange;
            }

            onWheel: wheel => {
                const delta = wheel.pixelDelta.y !== 0
                    ? wheel.pixelDelta.y
                    : wheel.angleDelta.y / 120 * control.wheelStep;
                control.scrollBy(-delta);
                scrollBar.poke();
                wheel.accepted = true;
            }
        }
    }

    Item {
        id: scrollGutter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: control.gutterWidth
        z: 11

        HoverHandler {
            id: gutterHover
        }
    }
}
