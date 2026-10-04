import QtQuick
import QtQuick.Window
import QtQuick.Effects
import Lib.Controls
import Lib.Theme

Window {
    id: root
    visible: true
    color: Theme.color.dark
    width: 960
    height: 640
    title: qsTr("Hello, World!")

    Rectangle {
        x: leftDrawer.width
        width: parent.width - leftDrawer.width
        height: parent.height
        color: Theme.color.dark

        PainterCanvas {
            anchors.fill: parent
            alphaBlending: true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        HorizontalDrawer {
            id: leftDrawer
            size: 0
            sizeMin: 100
            sizeMax: root.width > 500 ? root.width / 2 : 250
            height: root.height
            z: 8

            color: Theme.color.medium

            RectangularShadow {
                anchors.fill: parent
                visible: leftDrawer.width > 0
                radius: parent.radius
                color: "#20000000"
                blur: 10
                spread: 2
                offset: Qt.point(2, 0)
            }
        }
    }
}
