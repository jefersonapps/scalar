import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPopover {
    id: root
    objectName: "geometryOptions"
    property var canvas
    property real maximumHeight: Theme.windowHeight
    readonly property bool compassMode: canvas.tool === "compass"
    width: Theme.propertiesWidth + Theme.xxl
    contentItem: ScrollView {
        id: scroll
        contentWidth: availableWidth
        implicitHeight: Math.min(form.implicitHeight,root.maximumHeight)
        clip: true; ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout {
        id: form
        width: scroll.availableWidth
        spacing: Theme.sm
        Text { text: root.compassMode ? "Compasso" : "Régua"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
        Text { text: root.compassMode ? "Raio (mm)" : "Comprimento (mm)"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.Field { objectName: "geometryMeasure"; Layout.fillWidth: true; text: root.compassMode ? root.canvas.compassRadius.toFixed(1) : root.canvas.rulerLength.toFixed(1); validator: DoubleValidator { bottom: root.compassMode ? 1 : 20; top: 500; locale: "en_US" } onEditingFinished: if(acceptableInput) { if(root.compassMode) root.canvas.compassRadius=Number(text); else root.canvas.rulerLength=Number(text) } }
        Text { visible: !root.compassMode; text: "Ângulo (graus)"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.Field { visible: !root.compassMode; Layout.fillWidth: true; text: root.canvas.rulerAngle.toFixed(1); validator: DoubleValidator { bottom: -360; top: 360; locale: "en_US" } onEditingFinished: if(acceptableInput) root.canvas.rulerAngle=Number(text) }
        C.ModernCheckBox { objectName: "rulerSnapToggle"; visible: !root.compassMode; text: "Snap nas bordas da régua"; checked: root.canvas.rulerSnap; onToggled: root.canvas.rulerSnap=checked }
        Text { visible: !root.compassMode; text: "Distância para snap · " + root.canvas.rulerSnapDistance.toFixed(1) + " mm"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.ModernSlider { visible: !root.compassMode; Layout.fillWidth: true; from: .5; to: 10; value: root.canvas.rulerSnapDistance; onMoved: root.canvas.rulerSnapDistance=value }
        Text { Layout.fillWidth: true; text: root.compassMode ? "Arraste a dobradiça ou a haste metálica para mover. Arraste a haste azul para abrir e girar, mantendo a ponta seca fixa. Arraste a ponta do lápis para desenhar: uma volta completa cria um círculo; uma volta parcial mantém o arco." : "Arraste o corpo para mover, o handle esquerdo para girar e o direito para ajustar o comprimento. Escreva perto de uma borda com a caneta para traçar uma reta."; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.ActionButton { objectName: "drawCompassCircleButton"; visible: root.compassMode; Layout.fillWidth: true; text: "Traçar circunferência"; onClicked: { root.canvas.drawCompassCircle(); root.close() } }
        C.ActionButton { Layout.fillWidth: true; text: "Centralizar ferramentas"; onClicked: { root.canvas.centerGeometryTools(); root.close() } }
        C.ActionButton { Layout.fillWidth: true; text: "Ocultar " + (root.compassMode ? "compasso" : "régua"); onClicked: { if(root.compassMode) root.canvas.compassVisible=false; else root.canvas.rulerVisible=false; root.canvas.tool="pen"; root.close() } }
    }
    }
}
