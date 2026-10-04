import QtQuick
import QtQuick.Shapes
import Lib.Theme

Item {
    id: control

    property color color: Theme.color.accent
    property int thickness: 2
    property bool running: true

    implicitWidth: 14
    implicitHeight: 14

    Shape {
        id: ring
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: control.color
            strokeWidth: control.thickness
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap

            PathAngleArc {
                centerX: control.width / 2
                centerY: control.height / 2
                radiusX: Math.max(0, control.width / 2 - control.thickness / 2)
                radiusY: Math.max(0, control.height / 2 - control.thickness / 2)
                startAngle: -90
                sweepAngle: 270
            }
        }

        RotationAnimation on rotation {
            running: control.running && control.visible
            loops: Animation.Infinite
            from: 0
            to: 360
            duration: 900
        }
    }
}
