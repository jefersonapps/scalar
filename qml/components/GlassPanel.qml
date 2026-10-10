import QtQuick
import "../theme"
Rectangle {
    color: Theme.surface
    radius: Theme.radiusFloating
    border.color: Theme.border
    border.width: 1
    Rectangle {
        anchors.fill: parent
        anchors.topMargin: Theme.xs
        anchors.bottomMargin: -Theme.xs
        color: Theme.shadow
        radius: parent.radius
        z: -1
        visible: !App.reducedEffects
    }
}
