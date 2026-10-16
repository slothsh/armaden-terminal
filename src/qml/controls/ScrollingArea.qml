import QtQuick
import QtQuick.Window
import Lib.Interaction
import Lib.Theme

Item {
    id: control

    InteractionScope.domain: control.interactionEnabled ? control.interactionDomain : ""
    InteractionScope.role: control.interactionEnabled ? "scroll-area" : ""
    InteractionScope.scrollable: true
    InteractionScope.hitTestable: control.interactionEnabled

    property string interactionDomain: "app.scroll"
    property bool interactionEnabled: true

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
    property bool horizontalScrollEnabled: false
    property real contentWidthHint: -1
    property real contentHeightHint: -1

    property alias contentY: flick.contentY
    property alias contentX: flick.contentX
    property alias contentHeight: flick.contentHeight
    property alias contentWidth: flick.contentWidth
    property alias viewportHeight: flick.height
    property alias viewportWidth: flick.width

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: control.horizontalScrollEnabled && control.contentWidthHint >= 0
            ? Math.max(width, control.contentWidthHint)
            : width
        contentHeight: control.contentHeightHint >= 0
            ? control.contentHeightHint + control.bottomPadding
            : contentColumn.height + control.bottomPadding

        onContentHeightChanged: {
            Qt.callLater(function() {
                if (control.stickToBottom && control.atBottom()) {
                    control.snapToEnd();
                }
            });
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

    function scrollBy(delta) {
        flick.contentY = Math.max(flick.originY, Math.min(flick.contentY + delta, control.maxContentY()));
        scrollBar.poke();
    }

    function adjustBy(delta) {
        flick.contentY = Math.max(flick.originY, Math.min(flick.contentY + delta, control.maxContentY()));
    }

    function maxContentX() {
        return flick.originX + Math.max(0, flick.contentWidth - flick.width);
    }

    function scrollXBy(delta) {
        flick.contentX = Math.max(flick.originX, Math.min(flick.contentX + delta, control.maxContentX()));
        hScrollBar.poke();
    }

    signal interactionScrollRequested(real delta)

    onInteractionScrollRequested: delta => control.scrollBy(-delta * control.wheelStep)

    function handleWheel(wheel) {
        const delta = wheel.pixelDelta.y !== 0
            ? wheel.pixelDelta.y
            : wheel.angleDelta.y / 120 * control.wheelStep;
        control.scrollBy(-delta);
        wheel.accepted = true;
    }

    WheelHandler {
        objectName: "scrollAreaWheelHandler"
        target: null
        onWheel: wheel => control.handleWheel(wheel)
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

    Rectangle {
        id: hScrollBar
        anchors.bottom: parent.bottom
        anchors.bottomMargin: control.barRightMargin
        height: control.barWidth
        radius: height / 2
        color: control.barColor
        z: 10
        visible: control.horizontalScrollEnabled && flick.contentWidth > flick.width
        readonly property bool revealed: Window.active && (hGutterHover.hovered || hDragArea.pressed)
        opacity: 0
        readonly property real trackLeft: control.barTopMargin
        readonly property real trackWidth: Math.max(0, control.width - control.barTopMargin - control.barBottomMargin)
        readonly property real contentRange: Math.max(0, flick.contentWidth - flick.width)
        width: Math.min(trackWidth, Math.max(control.barMinHeight, trackWidth * flick.width / Math.max(1, flick.contentWidth)))
        x: trackLeft + (contentRange <= 0
            ? 0
            : (flick.contentX - flick.originX) / contentRange * (trackWidth - width))

        onRevealedChanged: {
            if (revealed) {
                hHideTimer.stop()
                hHideAnimation.stop()
                hScrollBar.opacity = 1
            } else {
                hHideTimer.restart()
            }
        }

        function poke() {
            hHideTimer.stop()
            hHideAnimation.stop()
            hScrollBar.opacity = 1
            hHideTimer.restart()
        }

        Timer {
            id: hHideTimer
            interval: control.hideDelay
            onTriggered: {
                if (!hScrollBar.revealed) {
                    hHideAnimation.restart()
                }
            }
        }

        NumberAnimation {
            id: hHideAnimation
            target: hScrollBar
            property: "opacity"
            to: 0
            duration: control.hideDuration
            easing.type: Easing.Linear
        }

        MouseArea {
            id: hDragArea
            anchors.fill: parent
            enabled: hScrollBar.opacity > 0
            cursorShape: Qt.PointingHandCursor

            property real grab: 0

            onPressed: mouse => {
                hDragArea.grab = mapToItem(flick, mouse.x, mouse.y).x - (hScrollBar.x - flick.x);
            }

            onPositionChanged: mouse => {
                if (!hDragArea.pressed) {
                    return;
                }

                const travel = Math.max(1, hScrollBar.trackWidth - hScrollBar.width);
                const left = mapToItem(flick, mouse.x, mouse.y).x - hDragArea.grab;
                const ratio = Math.max(0, Math.min(1, (left - hScrollBar.trackLeft) / travel));
                flick.contentX = flick.originX + ratio * hScrollBar.contentRange;
            }

            onWheel: wheel => {
                const delta = wheel.pixelDelta.x !== 0
                    ? wheel.pixelDelta.x
                    : wheel.angleDelta.x / 120 * control.wheelStep;
                control.scrollXBy(-delta);
                hScrollBar.poke();
                wheel.accepted = true;
            }
        }
    }

    Item {
        id: hScrollGutter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: control.gutterWidth
        z: 11
        visible: control.horizontalScrollEnabled

        HoverHandler {
            id: hGutterHover
        }
    }
}
