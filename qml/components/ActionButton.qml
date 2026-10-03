import QtQuick
import QtQuick.Controls
import "../theme"
AbstractButton {
    id: root
    property bool primary: false
    implicitWidth: Math.max(Theme.touch, label.implicitWidth + Theme.xxl)
    implicitHeight: Theme.touch
    hoverEnabled: true
    Accessible.name: text
    background: Rectangle {
        radius: Theme.radiusMedium
        color: root.primary ? Theme.accent : root.down ? Theme.pressed : root.hovered ? Theme.hover : Theme.surface
        border.width: root.primary ? 0 : 1
        border.color: Theme.border
        opacity: root.enabled ? (root.down ? 0.8 : 1) : 0.4
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    contentItem: Text {
        id: label
        text: root.text
        font.pixelSize: Theme.body
        font.weight: root.primary ? Font.DemiBold : Font.Medium
        color: root.primary ? Theme.accentText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
