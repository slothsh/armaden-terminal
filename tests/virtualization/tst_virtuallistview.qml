import QtQuick
import QtQuick.Window
import QtTest
import Lib.Interaction

TestCase {
    id: testCase

    name: "VirtualListView"
    when: windowShown

    function sizeFor(index) {
        return 40 + (index % 3) * 20
    }

    function totalContentHeight() {
        let total = 0
        for (let i = 0; i < 60; ++i) {
            total += sizeFor(i)
        }
        return total
    }

    function fillModel() {
        listModel.clear()
        for (let i = 0; i < 60; ++i) {
            listModel.append({text: "row " + i, size: sizeFor(i)})
        }
    }

    function resetView() {
        view.contentY = 0
        view.followTail = false
        fillModel()
    }

    ListModel {
        id: listModel
    }

    Window {
        id: hostWindow
        width: 320
        height: 320
        visible: true

        VirtualListView {
            id: view

            anchors.fill: parent
            model: listModel
            contentY: 0
            estimatedItemHeight: 60
            cacheBuffer: 320
            sizeHint: function(index, width, model) {
                return 40 + (index % 3) * 20
            }

            delegate: Component {
                Rectangle {
                    id: delegate

                    property string text: ""
                    property real size: 60
                    property int index: -1

                    objectName: "delegate-" + index
                    width: view.width
                    height: size
                    color: "steelblue"

                    Text {
                        anchors.centerIn: parent
                        text: delegate.text
                    }
                }
            }

            Connections {
                target: view

                function onScrollAdjustmentRequested(delta) {
                    view.contentY = Math.max(0, view.contentY + delta)
                }
            }
        }
    }

    function init() {
        resetView()
        waitForRendering(view)
        tryVerify(() => view.contentHeight === totalContentHeight())
    }

    function test_contentHeight_from_estimates() {
        compare(view.count, 60)
        compare(view.contentHeight, totalContentHeight())
        verify(view.visibleRatio > 0 && view.visibleRatio < 1)
    }

    function test_mounted_count_stays_bounded() {
        view.contentY = 3000
        waitForRendering(view)
        tryVerify(() => view.mountedCount > 0)
        verify(view.mountedCount >= 5, "expected tail delegates mounted: " + view.mountedCount)
        verify(view.mountedCount <= 30, "mounted count not bounded after scroll: " + view.mountedCount)
        verify(view.itemForIndex(55) !== null, "expected index 55 mounted near tail")
        verify(view.itemForIndex(0) === null, "index 0 should be unmounted at tail")
        compare(view.contentHeight, totalContentHeight())
    }

    function test_follow_tail_pins_on_append() {
        view.followTail = true
        view.contentY = view.contentHeight - view.height
        tryVerify(() => view.atEnd)

        listModel.append({text: "tail", size: 60})
        waitForRendering(view)
        wait(300)
        tryVerify(() => view.atEnd)

        const expected = view.contentHeight - view.height
        verify(Math.abs(view.contentY - expected) <= 1,
            "contentY " + view.contentY + " != tail " + expected)
    }

    function test_anchor_preserved_when_heights_change_above() {
        view.contentY = 1500
        waitForRendering(view)
        wait(200)

        const anchoredIndex = view.indexAt(1500)
        const anchoredOffset = 1500 - view.itemOffset(anchoredIndex)
        verify(anchoredIndex >= 20, "expected mid-list anchor, got " + anchoredIndex)

        listModel.setProperty(anchoredIndex - 5, "size", 200)
        waitForRendering(view)
        wait(300)

        const desired = view.itemOffset(anchoredIndex) + anchoredOffset
        verify(Math.abs(view.contentY - desired) <= 1,
            "anchor drifted: contentY " + view.contentY + " expected " + desired)
    }

    function test_offscreen_data_changes_do_not_move_content() {
        view.contentY = 1500
        waitForRendering(view)
        wait(200)
        const contentYBefore = view.contentY

        listModel.setProperty(2, "size", 260)
        waitForRendering(view)
        wait(300)

        verify(Math.abs(view.contentY - contentYBefore) <= 1,
            "offscreen resize moved viewport: " + view.contentY + " vs " + contentYBefore)
    }
}