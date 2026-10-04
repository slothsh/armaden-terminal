import QtQuick
import QtQuick.Window
import Lib.Controls

DropdownMenu {
    id: control

    function openAt(px, py) {
        const host = control.parent
        const win = host ? host.Window.window : null
        if (win && host) {
            const origin = host.mapToItem(win.contentItem, 0, 0)
            control.x = Math.max(0, Math.min(px, win.width - origin.x - control.width))
            control.y = Math.max(0, Math.min(py, win.height - origin.y - control.height))
        } else {
            control.x = px
            control.y = py
        }
        control.open()
    }
}
