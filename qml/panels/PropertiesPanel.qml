import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPanel {
    id: root
    property var canvas
    readonly property bool imageSelected: canvas && canvas.selectionName === "Imagem"
    readonly property bool fillEditable: canvas && ["Círculo", "Elipse", "Triângulo", "Retângulo", "Quadrado", "Polígono"].indexOf(canvas.selectionName) >= 0
    implicitWidth: Theme.propertiesWidth
    implicitHeight: form.implicitHeight+Theme.lg*2
    ColumnLayout {
        id: form
        anchors.fill: parent; anchors.margins: Theme.lg; spacing: Theme.sm
        Text { text: root.canvas ? root.canvas.selectionName : "Seleção"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
        Text { text: "Arraste para mover · handles para editar"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { visible: !root.imageSelected; text: "Cor do contorno"; color: Theme.secondary; font.pixelSize: Theme.caption }
        Flow {
            visible: !root.imageSelected; Layout.fillWidth: true; Layout.preferredHeight: childrenRect.height; spacing: Theme.xs
            Repeater {
                model: ["#263345", "#167b69", "#397ce0", "#cc5364", "#ffffff"]
                C.ColorButton { required property string modelData; swatch: modelData; selected: root.canvas && swatch === root.canvas.selectedBorderColor; onClicked: root.canvas.setSelectedColor(swatch) }
            }
        }
        Text { visible: !root.imageSelected; text: "Espessura da borda"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.ModernSlider { Layout.fillWidth: true; visible: !root.imageSelected; from: 0.2; to: 3; value: root.canvas ? root.canvas.selectedWidth : 0.85; onPressedChanged: { if(!pressed) root.canvas.setSelectedWidth(value) } }
        Text { visible: root.fillEditable; text: "Cor do preenchimento"; color: Theme.secondary; font.pixelSize: Theme.caption }
        Flow {
            objectName: "fillColorPalette"
            visible: root.fillEditable; Layout.fillWidth: true; Layout.preferredHeight: childrenRect.height; spacing: Theme.xs
            Repeater {
                model: ["#263345", "#167b69", "#397ce0", "#cc5364", "#ffffff"]
                C.ColorButton {
                    required property string modelData
                    required property int index
                    objectName: "fillColor_" + index
                    swatch: modelData; selected: root.canvas && swatch === root.canvas.selectedFillColor
                    onClicked: root.canvas.setSelectedFillColor(swatch)
                }
            }
        }
        Text { visible: root.fillEditable; text: "Opacidade do preenchimento"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.ModernSlider { Layout.fillWidth: true; visible: root.fillEditable; from: 0; to: 1; value: root.canvas ? root.canvas.selectedFill : 0.1; onPressedChanged: { if(!pressed) root.canvas.setSelectedFill(value) } }
        C.ActionButton { text: "Reconhecer forma"; Layout.fillWidth: true; visible: root.canvas && root.canvas.selectionName === "Traço"; enabled: visible; onClicked: root.canvas.recognizeSelection() }
        RowLayout {
            C.IconButton { iconName: "duplicate"; label: "Duplicar (Ctrl+D)"; onClicked: root.canvas.duplicateSelection() }
            C.IconButton { iconName: "trash"; label: "Excluir (Delete)"; onClicked: root.canvas.deleteSelection() }
        }
    }
}
