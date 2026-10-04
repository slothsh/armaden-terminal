pragma Singleton

import QtQuick
import QtQml

QtObject {
    property QtObject font: QtObject {}
    property QtObject color: QtObject {
        // Base
        property color medium: "#262626"
        property color dark: "#181818"

        // Border
        property color border: "#242424"

        // Splits & Resizers
        property color splitHandle: "#324252"
    }
}
