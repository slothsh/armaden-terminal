import QtQuick
import Lib.Theme

Rectangle {
    id: control

    implicitHeight: 32
    color: Theme.color.medium

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.color.borderStrong
    }
}
