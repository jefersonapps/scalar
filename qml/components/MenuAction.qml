import QtQuick
import QtQuick.Controls
import "../theme"
AbstractButton {
    id: root
    property string iconName: ""
    implicitWidth: 240
    implicitHeight: 40
    leftPadding: Theme.sm
    rightPadding: Theme.sm
    hoverEnabled: true
    Accessible.name: text
    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.down ? Theme.pressed : root.hovered || root.visualFocus ? Theme.hover : "transparent"
        border.width: root.visualFocus ? Theme.focusBorder : 0
        border.color: Theme.accent
    }
    contentItem: Item {
        implicitHeight: Theme.icon
        opacity: root.enabled ? 1 : .4
        Icon { id: icon; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; name: root.iconName; color: Theme.secondary }
        Text {
            anchors.left: icon.right; anchors.leftMargin: Theme.md
            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            text: root.text; color: Theme.text; font.pixelSize: Theme.body
            elide: Text.ElideRight
        }
    }
}
