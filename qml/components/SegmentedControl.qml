import QtQuick
import QtQuick.Layouts
import "../theme"
Rectangle {
    id: root
    property var options: []
    property int currentIndex: 0
    signal selected(int index)
    implicitHeight: Theme.touch+Theme.sm
    color: Theme.background
    radius: Theme.radiusMedium
    RowLayout {
        anchors.fill: parent; anchors.margins: Theme.xs; spacing: Theme.xs
        Repeater {
            model: root.options
            ActionButton {
                required property int index
                required property string modelData
                text: modelData
                Layout.fillWidth: true
                implicitHeight: Theme.touch
                primary: root.currentIndex === index
                onClicked: root.selected(index)
            }
        }
    }
}
