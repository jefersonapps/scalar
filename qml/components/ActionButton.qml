import QtQuick
import QtQuick.Controls
import "../theme"
AbstractButton {
    id: root
    property bool primary: false
    property string iconName: ""
    property string tooltip: ""
    implicitWidth: Math.max(Theme.touch, label.implicitWidth + Theme.xxl)
    implicitHeight: Theme.touch
    hoverEnabled: true
    Accessible.name: text
    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.text: tooltip
    ToolTip.delay: Theme.tooltipDelay
    background: Rectangle {
        radius: Theme.radiusMedium
        color: root.primary ? Theme.accent : root.down ? Theme.pressed : root.checked ? Theme.accentSoft : root.hovered ? Theme.hover : Theme.surface
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
        leftPadding: root.iconName ? Theme.icon + Theme.sm : 0
        Icon { anchors.left: parent.left; anchors.leftMargin: Theme.md; anchors.verticalCenter: parent.verticalCenter; visible: root.iconName!==""; name: root.iconName; color: label.color }
    }
}
