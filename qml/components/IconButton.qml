import QtQuick
import QtQuick.Controls
import "../theme"
AbstractButton {
    id: root
    property string iconName: "pen"
    property string label: ""
    property bool selected: false
    property bool compact: false
    implicitWidth: compact ? Theme.controlHeight : Theme.touch
    implicitHeight: compact ? Theme.controlHeight : Theme.touch
    padding: (implicitHeight-Theme.icon)/2
    hoverEnabled: true
    Accessible.name: label
    Accessible.role: Accessible.Button
    ToolTip.visible: hovered && label.length > 0
    ToolTip.text: label
    ToolTip.delay: Theme.tooltipDelay
    background: Rectangle {
        id: buttonBackground
        radius: Theme.radiusMedium
        color: root.selected ? Theme.selectionSurface : "transparent"
        border.width: root.selected ? 1 : 0
        border.color: Theme.selectionBorder
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        Rectangle {
            anchors.fill: parent; anchors.margins: buttonBackground.border.width
            radius: Math.max(0,buttonBackground.radius-buttonBackground.border.width)
            color: root.down || root.selected ? Theme.pressed : Theme.hover
            opacity: !root.enabled ? 0 : root.down ? 1 : root.hovered ? (root.selected ? .45 : 1) : 0
            Behavior on opacity { NumberAnimation { duration: root.down ? Theme.fast : Theme.hoverFade; easing.type: root.down ? Easing.OutCubic : Easing.InOutQuad } }
            Behavior on color { ColorAnimation { duration: Theme.fast } }
        }
    }
    contentItem: Icon {
        name: root.iconName
        color: root.selected ? Theme.accent : Theme.text
        Behavior on color { ColorAnimation { duration: Theme.normal } }
        opacity: root.enabled ? 1 : 0.3
        scale: root.down ? Theme.pressedIconScale : root.selected ? Theme.selectedIconScale : 1
        Behavior on scale { NumberAnimation { duration: Theme.fast } }
    }
}
