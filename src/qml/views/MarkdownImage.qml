import QtQuick
import Lib.Controls
import Lib.Theme

Item {
    id: control

    required property string sourceUrl
    property string altText: ""
    property string title: ""
    property real maximumWidth: 640
    property var imageLoader: null
    property bool renderEnabled: true
    property string localUrl: ""
    property bool animated: false
    property bool failed: false
    property bool requested: false
    property size intrinsicSize: Qt.size(0, 0)

    readonly property real displayWidth: intrinsicSize.width > 0
        ? Math.min(maximumWidth, intrinsicSize.width)
        : Math.min(maximumWidth, 240)
    readonly property real displayHeight: intrinsicSize.width > 0 && intrinsicSize.height > 0
        ? displayWidth * intrinsicSize.height / intrinsicSize.width
        : 48

    width: displayWidth
    height: displayHeight
    implicitWidth: width
    implicitHeight: height

    function request() {
        if (!control.renderEnabled || !control.imageLoader) {
            return
        }

        control.failed = false
        control.localUrl = control.imageLoader.cachedImageUrl(control.sourceUrl)
        if (control.localUrl.length > 0) {
            control.requested = false
            control.intrinsicSize = control.imageLoader.cachedImageSize(control.sourceUrl)
            return
        }

        if (!control.requested) {
            control.requested = true
            control.imageLoader.requestImage(control.sourceUrl)
        }
    }

    function release() {
        if (control.requested && control.imageLoader) {
            control.imageLoader.releaseImage(control.sourceUrl)
        }
        control.requested = false
    }

    Component.onCompleted: control.request()
    Component.onDestruction: control.release()
    onSourceUrlChanged: {
        control.release()
        control.localUrl = ""
        control.intrinsicSize = Qt.size(0, 0)
        control.request()
    }
    onRenderEnabledChanged: {
        if (control.renderEnabled) {
            control.request()
        } else {
            control.release()
        }
    }

    Connections {
        target: control.imageLoader
        ignoreUnknownSignals: true

        function onImageReady(source, url, isAnimated, dimensions) {
            if (source !== control.sourceUrl) {
                return
            }

            control.localUrl = url
            control.requested = false
            control.animated = isAnimated
            control.intrinsicSize = dimensions
            control.failed = false
        }

        function onImageFailed(source, reason) {
            if (source === control.sourceUrl) {
                control.requested = false
                control.failed = true
            }
        }
    }

    Image {
        anchors.fill: parent
        visible: control.localUrl.length > 0 && !control.animated
        source: control.localUrl
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    AnimatedImage {
        anchors.fill: parent
        visible: control.localUrl.length > 0 && control.animated
        source: control.localUrl
        asynchronous: true
        cache: true
        playing: visible && control.renderEnabled
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Rectangle {
        anchors.fill: parent
        visible: control.localUrl.length === 0
        color: Theme.color.medium
        radius: Theme.metric.radius
        border.color: Theme.color.borderStrong
        border.width: 1

        Text {
            anchors.fill: parent
            anchors.leftMargin: Theme.metric.spacing
            anchors.rightMargin: Theme.metric.spacing
            anchors.topMargin: Theme.metric.spacing
            anchors.bottomMargin: control.failed ? 28 : Theme.metric.spacing
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: control.failed && control.altText.length > 0
                ? control.altText + " — " + qsTr("Image could not be loaded")
                : (control.altText.length > 0 ? control.altText : (control.failed ? qsTr("Image could not be loaded") : qsTr("Loading image…")))
            color: Theme.color.textMuted
            font.pixelSize: 11
        }

        IconButton {
            visible: control.failed
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 2
            anchors.bottomMargin: 2
            iconName: "refresh-cw"
            accessibleLabel: qsTr("Retry loading image")
            onClicked: control.request()
        }
    }
}
