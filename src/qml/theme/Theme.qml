pragma Singleton

import QtQuick
import QtQml

QtObject {
    property QtObject font: QtObject {
        property int pointSize: 12
    }

    property QtObject metric: QtObject {
        property int radius: 4
        property int menuBarHeight: 34
        property int topBarHeight: 34
        property int spacing: 8
    }

    property QtObject color: QtObject {
        // Base
        property color medium: "#262626"
        property color mediumBright: "#2e2e2e"
        property color dark: "#181818"
        property color menu: "#1f1f1f"
        property color popup: "#313131"

        // Border
        property color border: "#242424"
        property color borderStrong: "#404040"

        // Accent
        property color accent: "#4a90d9"

        // Text & Icons
        property color text: "#e6e6e6"
        property color textDimmed: "#aaaaaa"
        property color textMuted: "#8a8a8a"

        // Interaction
        property color hover: "#33ffffff"
        property color active: "#40ffffff"
    }
}
