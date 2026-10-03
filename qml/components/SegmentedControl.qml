import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
Rectangle {
    id: root
    property var options: []
    property int currentIndex: 0
    signal selected(int index)
    implicitHeight: Theme.controlHeight
    color: Theme.background
    radius: Theme.radiusMedium
    RowLayout {
        anchors.fill: parent; anchors.margins: Theme.xs; spacing: Theme.xs
        Repeater {
            model: root.options
            AbstractButton {
                id: choice
                required property int index
                required property string modelData
                readonly property bool active: root.currentIndex === index
                text: modelData
                Layout.fillWidth: true; Layout.fillHeight: true
                hoverEnabled: true
                Accessible.name: text
                Accessible.role: Accessible.RadioButton
                Accessible.checked: active
                onClicked: root.selected(index)
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: choice.active ? Theme.accent : choice.down ? Theme.pressed : choice.hovered ? Theme.hover : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.fast } }
                }
                contentItem: Text {
                    text: choice.text; color: choice.active ? Theme.accentText : Theme.secondary
                    font.pixelSize: Theme.body; font.weight: choice.active ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
