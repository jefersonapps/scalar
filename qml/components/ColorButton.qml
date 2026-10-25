import QtQuick
import QtQuick.Controls
import "../theme"
AbstractButton {
    id: root
    property color swatch: "#263345"
    property bool selected: false
    property string shortcutLabel: ""
    implicitWidth: Theme.touch
    implicitHeight: Theme.touch + (shortcutLabel ? Theme.xs : 0)
    Accessible.name: "Cor " + swatch + (shortcutLabel ? " · Atalho " + shortcutLabel : "")
    ToolTip.visible: hovered && shortcutLabel.length > 0
    ToolTip.text: "Cor " + shortcutLabel + " · " + swatch
    ToolTip.delay: Theme.tooltipDelay
    background: Rectangle { radius: Theme.radiusMedium; color: root.hovered ? Theme.hover : "transparent"; border.color: root.selected ? Theme.accent : "transparent"; border.width: 2 }
    contentItem: Item {
        Rectangle { id: circle; width: Theme.xl; height: Theme.xl; anchors.horizontalCenter: parent.horizontalCenter; y: root.shortcutLabel ? Theme.xs : (parent.height-height)/2; radius: Theme.xl/2; color: root.swatch; border.color: Theme.border }
        Icon { name: "check"; width: Theme.lg; height: Theme.lg; anchors.centerIn: circle; visible: root.selected; color: root.swatch.hslLightness > 0.6 ? "#263345" : "#ffffff" }
        Text { visible: root.shortcutLabel.length > 0; text: root.shortcutLabel; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.xs; color: root.selected ? Theme.accent : Theme.secondary; font.pixelSize: 10 }
    }
    hoverEnabled: true
}
