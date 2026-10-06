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
        radius: Theme.radiusMedium
        color: root.down ? Theme.pressed : root.selected ? Theme.selectionSurface : root.hovered ? Theme.hover : "transparent"
        border.width: root.selected ? 1 : 0
        border.color: Theme.selectionBorder
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    contentItem: Icon {
        name: root.iconName
        color: root.selected ? Theme.accent : Theme.text
        opacity: root.enabled ? 1 : 0.3
        scale: root.down ? Theme.pressedIconScale : root.selected ? Theme.selectedIconScale : 1
        Behavior on scale { NumberAnimation { duration: Theme.fast } }
    }
}
