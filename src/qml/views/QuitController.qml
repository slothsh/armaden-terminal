import QtQuick
import Lib.Controls
import Lib.Interaction

Item {
    id: control

    property var chatController: null
    property var interactionController: null
    property bool quitConfirmed: false

    function requestQuit() {
        quitDialog.open()
    }

    function performQuit() {
        control.quitConfirmed = true
        control.chatController.cancelAll()
        if (!control.chatController.hasInFlight()) {
            Qt.quit()
        } else {
            quitDrainTimer.restart()
            quitTimeoutTimer.restart()
        }
    }

    ConfirmDialog {
        id: quitDialog
        interactionController: control.interactionController
        message: qsTr("Are you sure you want to exit? Any in-flight responses will be cancelled.")
        confirmText: qsTr("Quit")
        cancelText: qsTr("Cancel")
        onAccepted: control.performQuit()
    }

    Timer {
        id: quitDrainTimer
        interval: 100
        repeat: true
        onTriggered: {
            if (!control.chatController.hasInFlight()) {
                stop()
                quitTimeoutTimer.stop()
                Qt.quit()
            }
        }
    }

    Timer {
        id: quitTimeoutTimer
        interval: 5000
        onTriggered: {
            quitDrainTimer.stop()
            Qt.quit()
        }
    }
}
