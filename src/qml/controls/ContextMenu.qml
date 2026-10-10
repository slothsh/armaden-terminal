import QtQuick
import Lib.Controls

DropdownMenu {
    id: control

    function openAt(px, py) {
        if (control.interactionController) {
            control.interactionController.positionLayer(control, px, py)
        } else {
            control.x = px
            control.y = py
        }
        control.open()
    }
}
