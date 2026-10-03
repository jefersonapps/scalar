import QtQuick
import QtQuick.Controls
import "../theme"
AbstractButton {
    id: root
    property color swatch: "#263345"
    property bool selected: false
    implicitWidth: Theme.touch
    implicitHeight: Theme.touch
    Accessible.name: "Cor " + swatch
    background: Rectangle { radius: Theme.radiusMedium; color: root.hovered ? Theme.hover : "transparent"; border.color: root.selected ? Theme.accent : "transparent"; border.width: 2 }
    contentItem: Item {
        Rectangle { width: Theme.xl; height: Theme.xl; anchors.centerIn: parent; radius: Theme.xl/2; color: root.swatch; border.color: Theme.border }
        Icon { name: "check"; width: Theme.lg; height: Theme.lg; anchors.centerIn: parent; visible: root.selected; color: root.swatch.hslLightness > 0.6 ? "#263345" : "#ffffff" }
    }
    hoverEnabled: true
}
