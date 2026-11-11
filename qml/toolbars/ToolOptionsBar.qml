import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
Rectangle {
    id: root
    objectName: "toolOptionsBar"
    property var canvas
    readonly property bool textPopupOpen: textControls.popupOpen
    property var textEditor
    readonly property bool textContext: tool==="text" || (textEditor && textEditor.active) || (canvas.selectionName==="Texto" && canvas.selectedCount===1)
    property var palette
    readonly property string tool: canvas.tool
    readonly property bool marker: tool === "marker"
    readonly property bool shapes: ["line","circle","ellipse","triangle","rectangle"].indexOf(tool)>=0
    readonly property bool ink: !textContext && (tool === "pen" || marker || shapes || tool === "compass")
    readonly property bool geometry: tool === "ruler" || tool === "compass"
    readonly property real inkWidth: marker ? canvas.markerWidth : canvas.penWidth
    signal customColorRequested()
    implicitWidth: row.implicitWidth+16
    color: "transparent"
    height: 40
    clip: true
    Flickable {
        id: scroll
        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 0; anchors.topMargin: 4; anchors.bottomMargin: 4
        contentWidth: row.implicitWidth; contentHeight: height
        boundsBehavior: Flickable.StopAtBounds; clip: true
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
        MouseArea {
            z: 1
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            onWheel: wheel => {
                const maximum = Math.max(0, scroll.contentWidth-scroll.width)
                const pixels = wheel.pixelDelta
                const angles = wheel.angleDelta
                const delta = pixels.x || pixels.y || (angles.x || angles.y)/120*48
                scroll.contentX = Math.max(0, Math.min(maximum, scroll.contentX-delta))
                wheel.accepted = true
            }
        }
        RowLayout {
            id: row
            height: scroll.height; spacing: Theme.sm
            Text { text: root.textContext ? "Fonte" : ({pen:"Caneta",marker:"Marcador",eraser:"Borracha",select:"Seleção",hand:"Mover",text:"Texto",ruler:"Régua",compass:"Compasso",line:"Linha",circle:"Círculo",ellipse:"Elipse",triangle:"Triângulo",rectangle:"Retângulo"})[root.tool] || "Ferramenta"; font.pixelSize: Theme.caption; font.weight: Font.DemiBold; color: Theme.secondary }
            C.SelectField { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.shapes; Layout.preferredWidth: 132; model: ["Linha","Círculo","Elipse","Triângulo","Retângulo"]; currentIndex: ["line","circle","ellipse","triangle","rectangle"].indexOf(root.tool); onActivated: {root.canvas.tool=["line","circle","ellipse","triangle","rectangle"][index];root.canvas.forceActiveFocus()} }
            C.ThicknessPresets { objectName: "toolbarThicknessPresets"; visible: root.ink; values: root.marker ? [1, 5.75, 10.5, 15.25, 20] : [0.1, 0.4, 1.6, 2.3, 3]; value: root.inkWidth; onPicked: thickness => { if(root.marker)root.canvas.markerWidth=thickness;else root.canvas.penWidth=thickness;root.canvas.forceActiveFocus() } }
            RowLayout {
                visible: root.ink && !root.textContext; spacing: 2
                Repeater {
                    model: root.palette.paletteColors
                    C.ColorButton { required property var modelData; required property int index; objectName: "toolbarInkColor_"+index; implicitWidth: 30; implicitHeight: 32; Layout.maximumHeight: 32; swatch: modelData; selected: Qt.colorEqual(root.palette.currentColor,swatch); onClicked: {root.palette.setColor(swatch);root.canvas.forceActiveFocus()} }
                }
                C.IconButton { implicitHeight: 32; Layout.maximumHeight: 32; objectName: "toolbarCustomColor"; iconName: "plus"; label: "Cor personalizada"; onClicked: root.customColorRequested() }
            }
            C.SelectField { implicitHeight: 32; Layout.maximumHeight: 32; objectName: "toolbarLineStyle"; visible: root.ink&&!root.marker; Layout.preferredWidth: 132; model: ["Contínuo","Tracejado","Pontilhado"]; currentIndex: ["solid","dashed","dotted"].indexOf(root.shapes ? root.canvas.shapeLineStyle : root.canvas.penLineStyle); onActivated: {if(root.shapes)root.canvas.shapeLineStyle=["solid","dashed","dotted"][index];else root.canvas.penLineStyle=["solid","dashed","dotted"][index];root.canvas.forceActiveFocus()} }
            Repeater {
                model: root.ink&&!root.marker&&(root.shapes?root.canvas.shapeLineStyle:root.canvas.penLineStyle)!=="solid" ? ((root.shapes?root.canvas.shapeLineStyle:root.canvas.penLineStyle)==="dotted" ? ["dotSpacing"] : ["dashLength","gapLength"]) : []
                RowLayout {
                    required property string modelData
                    Text { text: modelData==="dotSpacing" ? "Pontos" : modelData==="dashLength" ? "Traço" : "Intervalo"; color: Theme.secondary; font.pixelSize: Theme.caption }
                    C.Field { implicitHeight: 32; Layout.maximumHeight: 32; Layout.preferredWidth: 64; text: root.canvas[modelData].toFixed(1); validator: DoubleValidator { bottom: .1; top: 100; locale: "C" } onTextEdited: if(acceptableInput)root.canvas[modelData]=Number(text) }
                }
            }
            Text { visible: root.tool==="pen"; text: "Pressão"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.ModernSlider { implicitHeight: 32; Layout.maximumHeight: 32; objectName: "toolbarPressure"; visible: root.tool==="pen"; Layout.preferredWidth: 110; from: .3; to: 3; value: root.canvas.pressureGamma; onMoved: root.canvas.pressureGamma=value }
            Text { visible: root.marker; text: "Opacidade · "+Math.round(root.canvas.markerOpacity*100)+"%"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.ModernSlider { implicitHeight: 32; Layout.maximumHeight: 32; objectName: "toolbarMarkerOpacity"; visible: root.marker; Layout.preferredWidth: 130; from: .01; to: 1; value: root.canvas.markerOpacity; onMoved: root.canvas.markerOpacity=value }
            Text { visible: root.tool==="eraser"; text: "Raio · "+root.canvas.eraserRadius.toFixed(1)+" mm"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.ModernSlider { implicitHeight: 32; Layout.maximumHeight: 32; objectName: "toolbarEraserRadius"; visible: root.tool==="eraser"; Layout.preferredWidth: 180; from: .5; to: 12; value: root.canvas.eraserRadius; onMoved: root.canvas.eraserRadius=value }
            C.ModernCheckBox { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.tool==="eraser"; text: "Apagar formas"; checked: root.canvas.eraserShapes; onToggled: root.canvas.eraserShapes=checked }
            Text { visible: root.geometry; text: root.tool==="compass" ? "Raio (mm)" : "Comprimento (mm)"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.Field { implicitHeight: 32; Layout.maximumHeight: 32; objectName: "toolbarGeometryMeasure"; visible: root.geometry; Layout.preferredWidth: 72; text: root.tool==="compass" ? root.canvas.compassRadius.toFixed(1) : root.canvas.rulerLength.toFixed(1); validator: DoubleValidator { bottom: root.tool==="compass" ? 1 : 20; top: 500; locale: "C" } onEditingFinished: if(acceptableInput){if(root.tool==="compass")root.canvas.compassRadius=Number(text);else root.canvas.rulerLength=Number(text)} }
            Text { visible: root.tool==="ruler"; text: "Ângulo (°)"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.Field { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.tool==="ruler"; Layout.preferredWidth: 72; text: (Number(root.canvas.rulerGeometry.displayAngle.toFixed(1))%360).toFixed(1); validator: DoubleValidator { bottom: 0; top: 360; locale: "C" } onEditingFinished: if(acceptableInput)root.canvas.rulerAngle=-Number(text) }
            C.ModernCheckBox { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.tool==="ruler"; text: "Ajustar à borda"; checked: root.canvas.rulerSnap; onToggled: root.canvas.rulerSnap=checked }
            C.ModernSlider { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.tool==="ruler"; Layout.preferredWidth: 110; from: .5; to: 10; value: root.canvas.rulerSnapDistance; onMoved: root.canvas.rulerSnapDistance=value; Accessible.name: "Distância para ajuste à régua (mm)" }
            C.ModernCheckBox { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.tool==="compass"; text: "Marcar centro"; checked: App.sessionCompassMarkCenter; onToggled: App.sessionCompassMarkCenter=checked }
            C.ActionButton { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.tool==="compass"; text: "Traçar círculo"; onClicked: root.canvas.drawCompassCircle() }
            C.ActionButton { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.geometry; text: "Centralizar"; onClicked: root.canvas.centerGeometryTools() }
            C.ActionButton { implicitHeight: 32; Layout.maximumHeight: 32; visible: root.geometry; text: "Ocultar"; onClicked: {if(root.tool==="compass")root.canvas.compassVisible=false;else root.canvas.rulerVisible=false;root.canvas.tool="pen"} }
            Text { visible: root.tool==="select" && !root.textContext; text: "Arraste para selecionar · Shift para adicionar · da direita para a esquerda inclui contato parcial"; color: Theme.secondary; font.pixelSize: Theme.caption }
            Text { visible: root.tool==="hand"; text: "Arraste para mover o quadro"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.TextFormatControls {
                id: textControls
                visible: root.textContext
                editor: root.textEditor; canvas: root.canvas
            }

        }
    }
}
