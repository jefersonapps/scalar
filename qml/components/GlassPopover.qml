import QtQuick
import QtQuick.Controls
import "../theme"
Popup {
    property bool expanded: false
    onAboutToShow: expanded = true
    onAboutToHide: expanded = false
    padding: Theme.lg
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: GlassPanel { radius: Theme.radiusLarge }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.fast } }
}
