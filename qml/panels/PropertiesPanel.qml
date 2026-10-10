import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPanel {
    id: root
    objectName: "objectPropertiesPanel"
    property var canvas
    signal exportRequested(bool svg)
    readonly property bool imageSelected: canvas && canvas.selectionName === "Imagem"
    readonly property bool textSelected: canvas && canvas.selectionName === "Texto"
    readonly property bool contourEditable: !imageSelected && !textSelected
    readonly property bool triangleSelected: canvas && canvas.selectionName === "Triângulo"
    readonly property bool lineSelected: canvas && canvas.selectionName === "Linha"
    readonly property bool fillEditable: canvas && ["Círculo", "Elipse", "Triângulo", "Retângulo", "Quadrado", "Polígono", "Setor circular", "Ângulo reto"].indexOf(canvas.selectionName) >= 0
    implicitWidth: Theme.propertiesWidth
    implicitHeight: content.implicitHeight + Theme.lg * 2
    height: Math.min(implicitHeight, parent ? Math.max(160, parent.height - y - 90) : implicitHeight)
    color: Theme.surface
    ColumnLayout {
        id: content
        anchors.fill: parent; anchors.margins: Theme.lg; spacing: Theme.md
        ColumnLayout {
            Layout.fillWidth: true; spacing: Theme.xs
            Text { text: root.canvas ? root.canvas.selectionName : "Seleção"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
            Text { text: "Arraste para mover · use os pontos para editar"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
        Flickable {
            id: options
            Layout.fillWidth: true; Layout.fillHeight: true
            Layout.preferredHeight: form.implicitHeight
            readonly property real availableWidth: width - (verticalBar.visible ? verticalBar.width + Theme.xs : 0)
            clip: true; contentWidth: width; contentHeight: form.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { id: verticalBar; policy: ScrollBar.AsNeeded; active: options.contentHeight > options.height }
            ColumnLayout {
                id: form
                width: options.availableWidth; spacing: Theme.sm
                Text { visible: !root.imageSelected; text: root.textSelected ? "Texto" : "Contorno"; color: Theme.text; font.pixelSize: Theme.caption; font.weight: Font.DemiBold }
                Flow {
                    visible: !root.imageSelected; Layout.fillWidth: true; Layout.preferredHeight: childrenRect.height; spacing: Theme.xs
                    Repeater {
                        model: ["#263345", "#167b69", "#397ce0", "#cc5364", "#ffffff"]
                        C.ColorButton { width: 40; height: 40; required property string modelData; swatch: modelData; selected: root.canvas && swatch === root.canvas.selectedBorderColor; onClicked: root.canvas.setSelectedColor(swatch) }
                    }
                }
                Text { visible: root.contourEditable; text: "Estilo da linha"; color: Theme.secondary; font.pixelSize: Theme.caption }
                RowLayout {
                    visible: root.contourEditable; Layout.fillWidth: true; spacing: Theme.xs
                    Repeater {
                        model: [{label: "Contínuo", value: 0}, {label: "Tracejado", value: 1}, {label: "Pontilhado", value: 2}]
                        C.ActionButton {
                            id: patternButton
                            required property var modelData
                            objectName: "selectedPattern_" + modelData.value
                            text: modelData.label; tooltip: modelData.label
                            Layout.fillWidth: true; Layout.preferredWidth: 1; implicitWidth: 0; implicitHeight: 52
                            checked: root.canvas && root.canvas.selectedPattern === modelData.value
                            onClicked: root.canvas.setSelectedPattern(modelData.value)
                            contentItem: Item {
                                Row {
                                    id: linePreview
                                    width: parent.width - 24; height: 4; y: 14
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    spacing: patternButton.modelData.value === 0 ? 0 : patternButton.modelData.value === 2 ? (width - 15) / 4 : 4
                                    Repeater {
                                        model: patternButton.modelData.value === 0 ? 1 : patternButton.modelData.value === 1 ? 3 : 5
                                        Rectangle {
                                            width: patternButton.modelData.value === 2 ? 3 : (linePreview.width - (patternButton.modelData.value === 0 ? 0 : (patternButton.modelData.value === 1 ? 8 : 16))) / (patternButton.modelData.value === 0 ? 1 : patternButton.modelData.value === 1 ? 3 : 5)
                                            height: 3; radius: 2
                                            color: patternButton.checked ? Theme.accent : Theme.text
                                        }
                                    }
                                }
                                Text { anchors.bottom: parent.bottom; anchors.bottomMargin: 7; width: parent.width; text: patternButton.modelData.label; color: Theme.text; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 11 }
                            }
                        }
                    }
                }
                RowLayout {
                    visible: root.contourEditable; Layout.fillWidth: true
                    Text { text: "Espessura"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true }
                    Text { text: root.canvas ? root.canvas.selectedWidth.toFixed(2) + " mm" : ""; color: Theme.secondary; font.pixelSize: Theme.caption }
                }
                PatternSettings { canvas: root.canvas; forSelection: true; Layout.fillWidth: true; visible: root.contourEditable && pattern > 0 }
                C.ModernSlider { Layout.fillWidth: true; visible: root.contourEditable; from: 0.2; to: 3; value: root.canvas ? root.canvas.selectedWidth : 0.85; onPressedChanged: { if(!pressed) root.canvas.setSelectedWidth(value) } }
                Rectangle { visible: root.fillEditable; Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.topMargin: Theme.xs }
                Text { visible: root.fillEditable; text: "Preenchimento"; color: Theme.text; font.pixelSize: Theme.caption; font.weight: Font.DemiBold }
                Flow {
                    objectName: "fillColorPalette"
                    visible: root.fillEditable; Layout.fillWidth: true; Layout.preferredHeight: childrenRect.height; spacing: Theme.xs
                    Repeater {
                        model: ["#263345", "#167b69", "#397ce0", "#cc5364", "#ffffff"]
                        C.ColorButton { width: 40; height: 40; required property string modelData; required property int index; objectName: "fillColor_" + index; swatch: modelData; selected: root.canvas && swatch === root.canvas.selectedFillColor; onClicked: root.canvas.setSelectedFillColor(swatch) }
                    }
                }
                RowLayout {
                    visible: root.fillEditable; Layout.fillWidth: true
                    Text { text: "Opacidade"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true }
                    Text { text: root.canvas ? Math.round(root.canvas.selectedFill * 100) + "%" : ""; color: Theme.secondary; font.pixelSize: Theme.caption }
                }
                C.ModernSlider { Layout.fillWidth: true; visible: root.fillEditable; from: 0; to: 1; value: root.canvas ? root.canvas.selectedFill : 0.1; onPressedChanged: { if(!pressed) root.canvas.setSelectedFill(value) } }
                C.ActionButton { text: "Editar texto / LaTeX"; visible: root.textSelected; Layout.fillWidth: true; onClicked: root.canvas.editSelectedText() }
                C.ModernCheckBox { objectName: "showSectorAngleToggle"; visible: root.canvas && root.canvas.selectionName === "Setor circular"; text: "Mostrar ângulo"; checked: root.canvas && root.canvas.selectedShowAngle; onToggled: root.canvas.selectedShowAngle=checked; Layout.fillWidth: true }
                C.ActionButton { text: "Reconhecer forma"; Layout.fillWidth: true; visible: root.canvas && root.canvas.selectionName === "Traço"; onClicked: root.canvas.recognizeSelection() }
                Rectangle { visible: root.triangleSelected || root.lineSelected; Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.topMargin: Theme.xs }
                Text { visible: root.triangleSelected || root.lineSelected; text: "Construções geométricas"; color: Theme.text; font.pixelSize: Theme.caption; font.weight: Font.DemiBold }
                RowLayout {
                    visible: root.lineSelected; Layout.fillWidth: true
                    C.ActionButton { objectName: "constructParallelButton"; text: "Paralela"; Layout.fillWidth: true; onClicked: root.canvas.constructFromSelection("parallel") }
                    C.ActionButton { objectName: "constructPerpendicularButton"; text: "Perpendicular"; Layout.fillWidth: true; onClicked: root.canvas.constructFromSelection("perpendicular") }
                }
                C.ActionButton { objectName: "constructBisectorButton"; visible: root.lineSelected; text: "Mediatriz"; Layout.fillWidth: true; onClicked: root.canvas.constructFromSelection("bisector") }
                C.ActionButton { objectName: "constructIncircleButton"; visible: root.triangleSelected; text: "Circunferência inscrita"; tooltip: "Tangente aos três lados do triângulo"; Layout.fillWidth: true; onClicked: root.canvas.constructFromSelection("incircle") }
                C.ActionButton { objectName: "constructCircumcircleButton"; visible: root.triangleSelected; text: "Circunferência circunscrita"; tooltip: "Passa pelos três vértices do triângulo"; Layout.fillWidth: true; onClicked: root.canvas.constructFromSelection("circumcircle") }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border; Layout.topMargin: Theme.xs }
                Text { text: "Camadas"; color: Theme.text; font.pixelSize: Theme.caption; font.weight: Font.DemiBold }
                RowLayout {
                    Layout.fillWidth: true
                    C.ActionButton { objectName: "moveLayerForwardButton"; text: "À frente"; tooltip: "Mover para frente (Ctrl + seta para cima)"; iconName: "layerUp"; Layout.fillWidth: true; onClicked: root.canvas.moveSelectionLayer(true) }
                    C.ActionButton { objectName: "moveLayerBackwardButton"; text: "Atrás"; tooltip: "Mover para trás (Ctrl + seta para baixo)"; iconName: "layerDown"; Layout.fillWidth: true; onClicked: root.canvas.moveSelectionLayer(false) }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }
        RowLayout {
            Layout.fillWidth: true
            C.ActionButton { objectName: "exportSelectionPdfButton"; text: "Exportar PDF"; Layout.fillWidth: true; enabled: !App.exporting; onClicked: root.exportRequested(false) }
            C.ActionButton { objectName: "exportSelectionSvgButton"; text: "Exportar SVG"; Layout.fillWidth: true; enabled: !App.exporting; onClicked: root.exportRequested(true) }
        }
        RowLayout {
            Layout.fillWidth: true
            C.ActionButton { objectName: "duplicateSelectionButton"; text: "Duplicar"; iconName: "duplicate"; Layout.fillWidth: true; onClicked: root.canvas.duplicateSelection() }
            C.IconButton { iconName: "trash"; label: "Excluir (Delete)"; onClicked: root.canvas.deleteSelection() }
        }
    }
}
