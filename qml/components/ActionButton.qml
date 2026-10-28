import QtQuick
import QtQuick.Controls
import "../theme"
AbstractButton {
    id: root
    property bool primary: false
    property real cornerRadius: Theme.radiusMedium
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
        id: buttonBackground
        radius: root.cornerRadius
        color: root.primary ? Theme.accent : root.checked ? Theme.accentSoft : Theme.surface
        border.width: root.primary ? 0 : 1
        border.color: Theme.border
        opacity: root.enabled ? 1 : 0.4
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Rectangle {
            anchors.fill: parent; anchors.margins: buttonBackground.border.width
            radius: Math.max(0,buttonBackground.radius-buttonBackground.border.width)
            color: root.primary ? Theme.accentText : root.down ? Theme.pressed : Theme.hover
            opacity: !root.enabled ? 0 : root.down ? (root.primary ? .18 : 1) : root.hovered ? (root.primary ? .1 : root.checked ? .45 : 1) : 0
            Behavior on opacity { NumberAnimation { duration: root.down ? Theme.fast : Theme.hoverFade; easing.type: root.down ? Easing.OutCubic : Easing.InOutQuad } }
            Behavior on color { ColorAnimation { duration: Theme.fast } }
        }
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
