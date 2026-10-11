import QtQuick
import QtQuick.Window
import QtTest
import Lib.Controls

TestCase {
    id: testCase

    name: "ActionControls"
    when: windowShown
    property var activeControl: null

    QtObject {
        id: fakeActions
        property int actionRevision: 1
        property bool actionAvailable: true

        function actionEnabled(actionId, target, surface) {
            return actionAvailable
        }
    }

    Window {
        id: testWindow
        width: 320
        height: 240
        visible: true
    }

    Component {
        id: iconButtonComponent

        IconButton {
            iconName: "send"
            actionId: "prompt.send"
            interactionController: fakeActions
        }
    }

    Component {
        id: menuButtonComponent

        MenuButton {
            label: "Send"
            actionId: "prompt.send"
            interactionController: fakeActions
        }
    }

    Component {
        id: textButtonComponent

        TextButton {
            label: "Send"
            actionId: "prompt.send"
            interactionController: fakeActions
        }
    }

    SignalSpy {
        id: clickedSpy
        target: testCase.activeControl
        signalName: "clicked"
    }

    function verifyActionControl(component) {
        testCase.activeControl = component.createObject(testWindow)
        verify(testCase.activeControl)
        verify(testCase.activeControl.enabled)
        verify(testCase.activeControl.actionAllowed)

        fakeActions.actionAvailable = false
        fakeActions.actionRevision += 1
        tryVerify(function() { return !testCase.activeControl.actionAllowed })
        verify(testCase.activeControl.enabled)
        testCase.activeControl.activate()
        compare(clickedSpy.count, 0)

        fakeActions.actionAvailable = true
        fakeActions.actionRevision += 1
        tryVerify(function() { return testCase.activeControl.actionAllowed })
        verify(testCase.activeControl.enabled)
        testCase.activeControl.activate()
        compare(clickedSpy.count, 1)

        testCase.activeControl.enabled = false
        testCase.activeControl.activate()
        compare(clickedSpy.count, 1)

        testCase.activeControl.destroy()
        testCase.activeControl = null
        clickedSpy.clear()
    }

    function test_actionAvailabilityDoesNotDisableItsTarget() {
        verifyActionControl(iconButtonComponent)
        verifyActionControl(menuButtonComponent)
        verifyActionControl(textButtonComponent)
    }
}
