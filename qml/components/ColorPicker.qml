import QtQuick
import QtQuick.Layouts
import "../theme"
Item {
    id: root
    property color selectedColor: "#263345"
    property var colors: ["#263345", "#167b69", "#397ce0", "#cc5364", "#b98624", "#ffffff"]
    property bool singleRow: false
    property bool numbered: false
    implicitWidth: Theme.propertiesWidth
    implicitHeight: swatches.childrenRect.height
    signal picked(color value)
    Flow {
    id: swatches
    width: parent.width
    spacing: Theme.xs
    Repeater {
        model: root.colors
        ColorButton {
            required property var modelData
            required property int index
            objectName: "paletteColor_" + index
            width: root.singleRow ? Math.floor((root.width-Math.max(0,root.colors.length-1)*swatches.spacing)/Math.max(1,root.colors.length)) : implicitWidth
            shortcutLabel: root.numbered ? String(index+1) : ""
            swatch: modelData
            selected: Qt.colorEqual(root.selectedColor,swatch)
            onClicked: root.picked(swatch)
        }
    }
}
}
