import QtQuick
import QtQuick.Controls
import "../theme"
Popup {
    padding: Theme.lg
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: GlassPanel { radius: Theme.radiusLarge }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.fast } }
}
