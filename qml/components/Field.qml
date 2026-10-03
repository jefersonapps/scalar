import QtQuick
import QtQuick.Controls
import "../theme"
TextField {
    id: root
    implicitHeight: Theme.touch
    color: Theme.text
    placeholderTextColor: Theme.secondary
    font.pixelSize: Theme.body
    leftPadding: Theme.lg
    rightPadding: Theme.lg
    selectByMouse: true
    background: Rectangle {
        radius: Theme.radiusMedium
        color: Theme.background
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.accent : Theme.border
    }
}
