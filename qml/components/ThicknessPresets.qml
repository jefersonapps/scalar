import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
RowLayout {
    id: root
    property real buttonHeight: 32
    property var values: [0.1, 0.4, 1.6, 2.3, 3]
    property real value: 0.4
    signal picked(real thickness)
    spacing: 2
    Repeater {
        model: root.values.length
        AbstractButton {
            id: choice
            required property int index
            readonly property real thickness: root.values[index]
            readonly property bool selected: Math.abs(root.value-thickness)<0.001
            Accessible.checkable: true
            Accessible.checked: selected
            objectName: "thicknessPreset_"+index
            implicitWidth: 36; implicitHeight: root.buttonHeight
            hoverEnabled: true
            Accessible.name: "Espessura "+thickness.toLocaleString(Qt.locale(), 'f', 2)+" mm"
            ToolTip.visible: hovered; ToolTip.text: Accessible.name; ToolTip.delay: Theme.tooltipDelay
            background: Rectangle { radius: Theme.radiusSmall; color: choice.selected ? Theme.accentSoft : choice.hovered ? Theme.hover : "transparent"; border.color: choice.selected ? Theme.accent : Theme.border }
            contentItem: Item {
                Rectangle { anchors.centerIn: parent; width: 24; height: choice.index===0 ? 1 : choice.index===1 ? 2 : 1.5+choice.index*1.7; radius: height/2; color: Theme.text }
            }
            onClicked: root.picked(thickness)
        }
    }
}
