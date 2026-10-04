import QtQuick
import QtQuick.Controls
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

    function isHandleHovered(mouse: MouseEvent): int {
        switch (drawer.side) {
            case DrawerSide.Right: {
                return (drawer.width === 0 ? -8 : -2) <= mouse.x && mouse.x <= 4;
            }

            case DrawerSide.Left:
            default: {
                return Math.max(0, drawer.width - 2) <= mouse.x && mouse.x <= drawer.width + (drawer.width === 0 ? 8 : 2);
            }
        }
    }

    function handleAreaX(): int {
        switch (drawer.side) {
            case DrawerSide.Right: {
                return drawer.width === 0 ? -8 : -2;
            }

            case DrawerSide.Left:
            default: {
                return 0;
            }
        }
    }

    function handleAreaWidth(): int {
        return drawer.width + (drawer.width === 0 ? 8 : 2);
    }

    function shouldCollapse(mouse: MouseEvent): int {
        switch (drawer.side) {
            case DrawerSide.Right: {
                return mouse.x > drawer.size - drawer.sizeMin / 2;
            }

            case DrawerSide.Left:
            default: {
                return mouse.x < drawer.sizeMin / 2;
            }
        }
    }

    function drawerSize(mouse: MouseEvent): int {
        switch (drawer.side) {
            case DrawerSide.Right: {
                return Math.max(drawer.sizeMin, Math.min(drawer.size - mouse.x + 4, drawer.sizeMax));
            }

            case DrawerSide.Left:
            default: {
                return Math.max(drawer.sizeMin, Math.min(mouse.x, drawer.sizeMax));
            }
        }
    }

    MouseArea {
        id: handleArea
        anchors.top: drawer.top
        anchors.bottom: drawer.bottom

        Binding { target: parent.anchors; property: "left";  value: parent.left;  when: drawer.side === DrawerSide.Left }
        Binding { target: parent.anchors; property: "right"; value: parent.right; when: drawer.side === DrawerSide.Right }

        x: drawer.handleAreaX()
        width: drawer.handleAreaWidth()

        hoverEnabled: true
        cursorShape: drawer.handleHovered || drawer.handleEnabled ? Qt.SplitHCursor : Qt.ArrowCursor

        onPressed: {
            if (drawer.handleHovered) {
                drawer.handleEnabled = true;
            }
        }

        onReleased: {
            drawer.handleEnabled = false;
        }

        onPositionChanged: mouse => {
            drawer.handleHovered = drawer.isHandleHovered(mouse);
            if (drawer.handleEnabled) {
                if (drawer.shouldCollapse(mouse)) {
                    drawer.size = 0;
                } else {
                    drawer.size = drawer.drawerSize(mouse);
                }
            }
        }

        onExited: {
            drawer.handleHovered = false;
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
