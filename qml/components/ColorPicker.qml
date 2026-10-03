import QtQuick
import QtQuick.Layouts
import "../theme"
RowLayout {
    id: root
    property color selectedColor: "#263345"
    signal picked(color value)
    spacing: Theme.xs
    Repeater {
        model: ["#263345", "#167b69", "#397ce0", "#cc5364", "#b98624", "#ffffff"]
        ColorButton { required property string modelData; swatch: modelData; selected: root.selectedColor.toString() === modelData; onClicked: root.picked(swatch) }
    }
}
