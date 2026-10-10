import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPopover {
    id: root
    objectName: "shapeOptions"
    property var canvas
    width: Theme.propertiesWidth + Theme.xxl * 2
    contentItem: ColumnLayout {
        spacing: Theme.md
        Text { text: "Formas"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
        Text { text: "Contorno"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.SegmentedControl {
            objectName: "shapeLineStyleControl"
            Layout.fillWidth: true
            options: ["Contínuo", "Tracejado", "Pontilhado"]
            currentIndex: root.canvas ? ["solid", "dashed", "dotted"].indexOf(root.canvas.shapeLineStyle) : 0
            onSelected: index => root.canvas.shapeLineStyle = ["solid", "dashed", "dotted"][index]
        }
        PatternSettings { canvas: root.canvas; Layout.fillWidth: true }
        GridLayout {
            Layout.fillWidth: true; columns: 2; columnSpacing: Theme.xs; rowSpacing: Theme.xs
            Repeater {
                model: [{name: "Linha", tool: "line"}, {name: "Círculo", tool: "circle"}, {name: "Elipse", tool: "ellipse"}, {name: "Triângulo", tool: "triangle"}, {name: "Retângulo", tool: "rectangle"}]
                C.IconButton {
                    id: choice
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true; Layout.columnSpan: index === 4 ? 2 : 1
                    objectName: "shapeChoice_" + modelData.tool
                    iconName: modelData.tool; label: modelData.name; selected: root.canvas && root.canvas.tool === modelData.tool
                    contentItem: RowLayout {
                        spacing: Theme.sm
                        C.Icon { name: choice.iconName; color: Theme.text; Layout.preferredWidth: Theme.icon; Layout.preferredHeight: Theme.icon }
                        Text { text: choice.label; color: Theme.text; font.pixelSize: Theme.body; Layout.fillWidth: true }
                    }
                    onClicked: { root.canvas.tool = modelData.tool; root.close(); root.canvas.forceActiveFocus() }
                }
            }
        }
        Text { text: "Arraste para inserir. Caneta + segurar reconhece formas; Shift cria um contorno tracejado."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.caption }
    }
}
