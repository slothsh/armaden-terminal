import QtQuick
import QtQuick.Controls
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

    width: parent.width
    height: Math.max(0, Math.min(drawer.size, drawer.sizeMax))

    Binding {
        target: drawer
        property: "height"
        value: Math.max(0, Math.min(drawer.size, drawer.sizeMax))
        when: !drawer.handleHovered && !drawer.handleEnabled
    }

    Binding { target: drawer.anchors; property: "top";   value: parent.top;   when: drawer.side === DrawerSide.Top }
    Binding { target: drawer.anchors; property: "left";  value: parent.left;  when: drawer.side === DrawerSide.Top }
    Binding { target: drawer.anchors; property: "right"; value: parent.right; when: drawer.side === DrawerSide.Top }

    Binding { target: drawer.anchors; property: "bottom"; value: parent.bottom; when: drawer.side === DrawerSide.Bottom }
    Binding { target: drawer.anchors; property: "left";   value: parent.left;   when: drawer.side === DrawerSide.Bottom }
    Binding { target: drawer.anchors; property: "right";  value: parent.right;  when: drawer.side === DrawerSide.Bottom }

    function isHandleHovered(mouse: MouseEvent): int {
        switch (drawer.side) {
            case DrawerSide.Bottom: {
                return (drawer.height === 0 ? -8 : -2) <= mouse.y && mouse.y <= 4;
            }

            case DrawerSide.Top:
            default: {
                return Math.max(0, drawer.height - 2) <= mouse.y && mouse.y <= drawer.height + (drawer.height === 0 ? 8 : 2);
            }
        }
    }

    function handleAreaY(): int {
        switch (drawer.side) {
            case DrawerSide.Bottom: {
                return drawer.height === 0 ? -8 : -2;
            }

            case DrawerSide.Top:
            default: {
                return 0;
            }
        }
    }

    function handleAreaHeight(): int {
        return drawer.height + (drawer.height === 0 ? 8 : 2);
    }

    function shouldCollapse(mouse: MouseEvent): int {
        switch (drawer.side) {
            case DrawerSide.Bottom: {
                return mouse.y > drawer.size - drawer.sizeMin / 2;
            }

            case DrawerSide.Top:
            default: {
                return mouse.y < drawer.sizeMin / 2;
            }
        }
    }

    function drawerSize(mouse: MouseEvent): int {
        switch (drawer.side) {
            case DrawerSide.Bottom: {
                return Math.max(drawer.sizeMin, Math.min(drawer.size - mouse.y + 4, drawer.sizeMax));
            }

            case DrawerSide.Top:
            default: {
                return Math.max(drawer.sizeMin, Math.min(mouse.y, drawer.sizeMax));
            }
        }
    }

    MouseArea {
        id: handleArea
        anchors.left: drawer.left
        anchors.right: drawer.right

        Binding { target: parent.anchors; property: "top";    value: parent.top;    when: drawer.side === DrawerSide.Top }
        Binding { target: parent.anchors; property: "bottom"; value: parent.bottom; when: drawer.side === DrawerSide.Bottom }

        y: drawer.handleAreaY()
        height: drawer.handleAreaHeight()

        hoverEnabled: true
        cursorShape: drawer.handleHovered || drawer.handleEnabled ? Qt.SplitVCursor : Qt.ArrowCursor

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
        anchors.left: drawer.left
        anchors.right: drawer.right
        anchors.verticalCenter: drawer.side === DrawerSide.Top ? drawer.bottom : drawer.top;
        clip: false
        color: drawer.handleHovered || drawer.handleEnabled ? Theme.color.splitHandle : Theme.color.borderStrong

        y: drawer.handleAreaY()
        height: drawer.height === 0
            ? (drawer.handleHovered || drawer.handleEnabled ? 8 : 0)
            : (drawer.handleHovered || drawer.handleEnabled ? 4 : 1)
    }
}
