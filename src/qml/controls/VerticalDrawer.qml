import QtQuick
import Lib.Controls
import Lib.Theme

Rectangle {
    id: drawer

    required property int size
    required property int sizeMin
    required property int sizeMax

    property int side: DrawerSide.Bottom
    property bool handleEnabled: false
    property bool handleHovered: false
    property int handleThickness: 1
    property int handleHoverThickness: 1
    property int handleCollapsedThickness: 2

    property int lastOpenSize: size
    property int pressSize: 0

    onSizeChanged: {
        if (size > 0) {
            lastOpenSize = size;
        }
    }

    function toggle() {
        size = size > 0 ? 0 : Math.min(lastOpenSize, sizeMax);
    }

    function beginResize() {
        handleEnabled = true;
        pressSize = size;
    }

    function endResize() {
        handleEnabled = false;
        if (size === 0 && pressSize > 0) {
            lastOpenSize = pressSize;
        }
    }

    width: parent.width
    height: Math.max(0, Math.min(drawer.size, drawer.sizeMax))

    Binding {
        target: drawer
        property: "height"
        value: Math.max(0, Math.min(drawer.size, drawer.sizeMax))
        when: !drawer.handleHovered && !drawer.handleEnabled
    }

    Binding { target: drawer.anchors; property: "top";    value: parent.top;    when: drawer.side === DrawerSide.Top }
    Binding { target: drawer.anchors; property: "left";   value: parent.left;   when: drawer.side === DrawerSide.Top }
    Binding { target: drawer.anchors; property: "right";  value: parent.right;  when: drawer.side === DrawerSide.Top }

    Binding { target: drawer.anchors; property: "bottom"; value: parent.bottom; when: drawer.side === DrawerSide.Bottom }
    Binding { target: drawer.anchors; property: "left";   value: parent.left;   when: drawer.side === DrawerSide.Bottom }
    Binding { target: drawer.anchors; property: "right";  value: parent.right;  when: drawer.side === DrawerSide.Bottom }

    function handleAreaY(): int {
        switch (drawer.side) {
            case DrawerSide.Bottom: {
                return drawer.height === 0 ? -8 : -3;
            }

            case DrawerSide.Top:
            default: {
                return drawer.height === 0 ? 0 : drawer.height - 3;
            }
        }
    }

    function handleAreaHeight(): int {
        return drawer.height === 0 ? 8 : 6;
    }

    function shouldCollapse(y: int): bool {
        switch (drawer.side) {
            case DrawerSide.Bottom: {
                return y > drawer.size - drawer.sizeMin / 2;
            }

            case DrawerSide.Top:
            default: {
                return y < drawer.sizeMin / 2;
            }
        }
    }

    function drawerSize(y: int): int {
        switch (drawer.side) {
            case DrawerSide.Bottom: {
                return Math.max(drawer.sizeMin, Math.min(drawer.size - y + 4, drawer.sizeMax));
            }

            case DrawerSide.Top:
            default: {
                return Math.max(drawer.sizeMin, Math.min(y, drawer.sizeMax));
            }
        }
    }

    MouseArea {
        id: handleArea
        anchors.left: drawer.left
        anchors.right: drawer.right

        y: drawer.handleAreaY()
        height: drawer.handleAreaHeight()
        z: 16

        hoverEnabled: true
        cursorShape: Qt.SplitVCursor

        onEntered: {
            drawer.handleHovered = true;
        }

        onExited: {
            drawer.handleHovered = false;
        }

        onPressed: {
            drawer.beginResize();
        }

        onReleased: {
            drawer.endResize();
        }

        onPositionChanged: mouse => {
            if (!drawer.handleEnabled) {
                return;
            }

            const y = mouse.y + drawer.handleAreaY();
            if (drawer.shouldCollapse(y)) {
                drawer.size = 0;
            } else {
                drawer.size = drawer.drawerSize(y);
            }
        }
    }

    Rectangle {
        anchors.left: drawer.left
        anchors.right: drawer.right
        anchors.verticalCenter: drawer.side === DrawerSide.Top ? drawer.bottom : drawer.top;
        clip: false
        color: drawer.handleHovered || drawer.handleEnabled ? Theme.color.accent : Theme.color.borderStrong
        z: 15

        height: drawer.height === 0
            ? (drawer.handleHovered || drawer.handleEnabled ? drawer.handleCollapsedThickness : 0)
            : (drawer.handleHovered || drawer.handleEnabled ? drawer.handleHoverThickness : drawer.handleThickness)
    }
}
