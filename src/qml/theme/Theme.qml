pragma Singleton

import QtQuick
import QtQml

QtObject {
    property QtObject font: QtObject {}

    property QtObject metric: QtObject {
        property int radius: 4
        property int topBarHeight: 34
        property int spacing: 8
    }

    property QtObject color: QtObject {
        // Base
        property color medium: "#262626"
        property color dark: "#181818"
        property color menu: "#1f1f1f"

        // Border
        property color border: "#242424"
        property color borderStrong: "#333333"

        // Splits & Resizers
        property color splitHandle: "#324252"

        // Text & Icons
        property color text: "#e6e6e6"
        property color textMuted: "#8a8a8a"

        // Interaction
        property color hover: "#33ffffff"
    }
}
