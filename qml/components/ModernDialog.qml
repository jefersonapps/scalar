import QtQuick
import QtQuick.Controls
import "../theme"
Popup {
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Theme.dialogWidth,parent.width-Theme.xxl)
    height: Math.min(implicitHeight, parent.height-Theme.xxl)
    modal: true
    focus: true
    padding: Theme.xl
    closePolicy: Popup.CloseOnEscape
    background: GlassPanel { radius: Theme.radiusFloating }
    Overlay.modal: Rectangle { color: "#60202c38" }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.fast } }
}
