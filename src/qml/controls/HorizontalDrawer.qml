import QtQuick
import Lib.Controls
import Lib.Theme

Rectangle {
    id: drawer

    required property int size
    required property int sizeMin
    required property int sizeMax

    property int side: DrawerSide.Left
    property bool handleEnabled: false
    property bool handleHovered: false
    property int handleThickness: 1
    property int handleHoverThickness: 1
    property int handleCollapsedThickness: 2

    width: Math.max(0, Math.min(drawer.size, drawer.sizeMax))

    Binding {
        target: drawer
        property: "width"
        value: Math.max(0, Math.min(drawer.size, drawer.sizeMax))
        when: !drawer.handleHovered && !drawer.handleEnabled
    }

    Binding { target: drawer.anchors; property: "left";   value: parent.left;   when: drawer.side === DrawerSide.Left }
    Binding { target: drawer.anchors; property: "right";  value: parent.right;  when: drawer.side === DrawerSide.Right }

    function handleAreaX(): int {
        switch (drawer.side) {
            case DrawerSide.Right: {
                return drawer.width === 0 ? -8 : -3;
            }

            case DrawerSide.Left:
            default: {
                return drawer.width === 0 ? 0 : drawer.width - 3;
            }
        }
    }

    function handleAreaWidth(): int {
        return drawer.width === 0 ? 8 : 6;
    }

    function shouldCollapse(x: int): bool {
        switch (drawer.side) {
            case DrawerSide.Right: {
                return x > drawer.size - drawer.sizeMin / 2;
            }

            case DrawerSide.Left:
            default: {
                return x < drawer.sizeMin / 2;
            }
        }
    }

    function drawerSize(x: int): int {
        switch (drawer.side) {
            case DrawerSide.Right: {
                return Math.max(drawer.sizeMin, Math.min(drawer.size - x + 4, drawer.sizeMax));
            }

            case DrawerSide.Left:
            default: {
                return Math.max(drawer.sizeMin, Math.min(x, drawer.sizeMax));
            }
        }
    }

    MouseArea {
        id: handleArea
        anchors.top: drawer.top
        anchors.bottom: drawer.bottom

        x: drawer.handleAreaX()
        width: drawer.handleAreaWidth()
        z: 16

        hoverEnabled: true
        cursorShape: Qt.SplitHCursor

        onEntered: {
            drawer.handleHovered = true;
        }

        onExited: {
            drawer.handleHovered = false;
        }

        onPressed: {
            drawer.handleEnabled = true;
        }

        onReleased: {
            drawer.handleEnabled = false;
        }

        onPositionChanged: mouse => {
            if (!drawer.handleEnabled) {
                return;
            }

            const x = mouse.x + drawer.handleAreaX();
            if (drawer.shouldCollapse(x)) {
                drawer.size = 0;
            } else {
                drawer.size = drawer.drawerSize(x);
            }
        }
    }

    Rectangle {
        anchors.top: drawer.top
        anchors.bottom: drawer.bottom
        anchors.horizontalCenter: drawer.side === DrawerSide.Left ? drawer.right : drawer.left;
        clip: false
        color: drawer.handleHovered || drawer.handleEnabled ? Theme.color.splitHandle : Theme.color.borderStrong
        z: 15

        width: drawer.width === 0
            ? (drawer.handleHovered || drawer.handleEnabled ? drawer.handleCollapsedThickness : 0)
            : (drawer.handleHovered || drawer.handleEnabled ? drawer.handleHoverThickness : drawer.handleThickness)
    }
}
