import QtQuick
import "../components" as C
import "../theme"
C.ModernDialog {
    objectName: "settingsDialog"
    padding: Theme.lg
    contentItem: C.SettingsContent {}
}
