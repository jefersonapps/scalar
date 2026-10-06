import QtQuick
import QtQuick.Controls
import "../theme"
CheckBox {
    id: root
    implicitHeight: Theme.touch
    spacing: Theme.md
    padding: Theme.xs
    hoverEnabled: true
    indicator: Rectangle {
        x: root.leftPadding; y: (root.height-height)/2
        width: Theme.icon; height: width; radius: Theme.xs
        color: root.checked ? Theme.selectionSurface : root.hovered ? Theme.hover : Theme.background
        border.color: root.activeFocus ? Theme.accent : root.checked ? Theme.selectionBorder : Theme.border
        border.width: root.activeFocus ? Theme.focusBorder : Theme.hairline
        Icon { anchors.centerIn: parent; width: Theme.guideHandle; height: width; name: "check"; color: Theme.text; visible: root.checked }
    }
    contentItem: Text {
        leftPadding: root.indicator.width+root.spacing
        text: root.text; font.pixelSize: Theme.body; color: Theme.text
        verticalAlignment: Text.AlignVCenter
        opacity: root.enabled ? 1 : 0.5
    }
}
