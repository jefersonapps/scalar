import QtQuick
import QtQuick.Layouts
import "../theme"
Item {
    id: root
    property color selectedColor: "#263345"
    property var colors: ["#263345", "#167b69", "#397ce0", "#cc5364", "#b98624", "#ffffff"]
    implicitWidth: Theme.propertiesWidth
    implicitHeight: swatches.childrenRect.height
    signal picked(color value)
    Flow {
    id: swatches
    width: parent.width
    spacing: Theme.xs
    Repeater {
        model: root.colors
        ColorButton { required property var modelData; required property int index; objectName: "paletteColor_" + index; swatch: modelData; selected: root.selectedColor.toString() === swatch.toString(); onClicked: root.picked(swatch) }
    }
}
}
