import QtQuick
import "../theme"

Rectangle {
    property real angle: 0
    width: label.implicitWidth + Theme.sm * 2
    height: label.implicitHeight + Theme.xs * 2
    color: Theme.surface; border.color: Theme.accent; radius: Theme.xs
    Text { id: label; anchors.centerIn: parent; color: Theme.text; font.pixelSize: Theme.caption; text: parent.angle.toFixed(1).replace(/\.0$/, "") + "°" }
}
