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

    TopBar {
        id: topBar
        anchors.top: parent.top
        width: parent.width
        z: 9
        onMenuRequested: appMenu.open()
        onCloseRequested: Qt.quit()
    }

    Rectangle {
        x: leftDrawer.width
        y: topBar.height
        width: parent.width - leftDrawer.width
        height: parent.height - topBar.height
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

    DropdownMenu {
        id: appMenu
        x: Theme.metric.spacing
        y: topBar.height + Theme.metric.spacing

        MenuButton {
            width: appMenu.width - appMenu.padding * 2
            label: qsTr("About")
            onClicked: {
                appMenu.close()
                aboutDialog.open()
            }
        }

        Rectangle {
            width: appMenu.width - appMenu.padding * 2
            height: 1
            color: Theme.color.border
        }

        MenuButton {
            width: appMenu.width - appMenu.padding * 2
            label: qsTr("Quit")
            onClicked: Qt.quit()
        }
    }

    AboutDialog {
        id: aboutDialog
        title: qsTr("mindtosis")
        version: qsTr("Version 0.0.0")
        description: qsTr("A mindtosis application.")
    }
}
