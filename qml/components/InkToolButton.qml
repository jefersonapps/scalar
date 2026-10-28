import QtQuick
import "../theme"
ToolButton {
    id: root
    property color inkColor: "#263345"
    padding: Theme.xs
    contentItem: Item {
        Icon {
            objectName: root.iconName + "ToolIcon"
            name: root.iconName
            width: 22; height: 22
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: colorIndicator.top
            color: root.selected ? Theme.accent : Theme.text
            Behavior on color { ColorAnimation { duration: Theme.normal } }
            opacity: root.enabled ? 1 : 0.3
        }
        Rectangle {
            id: colorIndicator
            objectName: root.iconName + "ColorIndicator"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: 22; height: 7; radius: height/2
            color: root.inkColor
            border.width: Theme.hairline; border.color: Theme.border
            opacity: root.enabled ? 1 : 0.3
        }
    }
}
