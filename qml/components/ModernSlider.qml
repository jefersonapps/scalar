import QtQuick
import QtQuick.Controls
import "../theme"
Slider {
    id: root
    implicitHeight: Theme.touch
    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding+(root.availableHeight-height)/2
        width: root.availableWidth; height: Theme.xs; radius: Theme.xs
        color: Theme.border
        Rectangle { width: root.visualPosition*parent.width; height: parent.height; radius: parent.radius; color: Theme.accent }
    }
    handle: Rectangle {
        x: root.leftPadding+root.visualPosition*(root.availableWidth-width)
        y: root.topPadding+(root.availableHeight-height)/2
        width: Theme.xl; height: Theme.xl; radius: Theme.xl/2
        color: root.pressed ? Theme.accent : Theme.surface
        border.width: 2; border.color: Theme.accent
    }
}
