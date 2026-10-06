import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPopover {
    id: root
    objectName: "penOptions"
    width: Theme.propertiesWidth + padding*2
    property var canvas
    readonly property bool marker: canvas && canvas.tool === "marker"
    readonly property bool colorDialogOpen: customColor.visible
    closePolicy: colorDialogOpen ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    readonly property bool darkPage: App.pageColor.hslLightness < 0.5
    readonly property var inkColors: [darkPage ? Theme.inkNeutralOnDark : Theme.inkNeutralOnLight].concat(Theme.inkHues.map((h,i) => Qt.hsla(h,Theme.inkSaturations[i],darkPage ? Theme.inkLightnessOnDark[i] : Theme.inkLightnessOnLight[i],1)))
    C.CustomColorDialog { id: customColor; objectName: "penColorDialog"; onPreviewed: value => { if(root.canvas) root.canvas.penColor = value }; onChosen: value => { if(root.canvas) root.canvas.penColor = value } }
    contentItem: ScrollView {
        id: penScroll
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        implicitHeight: Math.min(form.implicitHeight,root.parent.height-Theme.topbarHeight-Theme.toolbarHeight-Theme.xxl*2)
        clip: true
        ColumnLayout {
        id: form
        width: penScroll.availableWidth
        spacing: Theme.md
        Text { text: root.marker ? "Seu marcador" : "Sua caneta"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
        C.ColorPicker { objectName: "penPalette"; Layout.fillWidth: true; colors: root.inkColors; selectedColor: root.canvas ? root.canvas.penColor : "#263345"; onPicked: value => { if(root.canvas) root.canvas.penColor = value } }
        C.ActionButton { objectName: "customPenColorButton"; text: "Cor personalizada"; Layout.fillWidth: true; onClicked: if(root.canvas) customColor.choose(root.canvas.penColor) }
        RowLayout {
            Text { text: "Espessura máxima"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true }
            Text { text: root.canvas ? (root.marker ? root.canvas.markerWidth : root.canvas.penWidth).toFixed(2) + " mm" : ""; color: Theme.text; font.pixelSize: Theme.caption }
        }
        C.ModernSlider { Layout.fillWidth: true; from: root.marker ? 1 : 0.2; to: root.marker ? 20 : 3; value: root.canvas ? (root.marker ? root.canvas.markerWidth : root.canvas.penWidth) : 0.40; onMoved: { if(root.canvas) { if(root.marker) root.canvas.markerWidth = value; else root.canvas.penWidth = value } } }
        RowLayout {
            visible: root.marker
            Text { text: "Opacidade"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true }
            Text { text: root.canvas ? Math.round(root.canvas.markerOpacity*100)+"%" : "10%"; color: Theme.text; font.pixelSize: Theme.caption }
        }
        C.ModernSlider { objectName: "markerOpacitySlider"; visible: root.marker; Layout.fillWidth: true; from: .01; to: 1; stepSize: .01; value: root.canvas ? root.canvas.markerOpacity : .10; onMoved: if(root.canvas) root.canvas.markerOpacity=value }
        C.SelectField {
            visible: !root.marker
            Layout.fillWidth: true; model: ["Contínuo", "Tracejado", "Pontilhado"]
            currentIndex: root.canvas ? ["solid","dashed","dotted"].indexOf(root.canvas.penLineStyle) : 0
            onActivated: if(root.canvas) root.canvas.penLineStyle = ["solid","dashed","dotted"][index]
        }
        RowLayout {
            visible: !root.marker && root.canvas && root.canvas.penLineStyle !== "solid"
            Repeater {
                model: root.canvas && root.canvas.penLineStyle === "dotted" ? ["dotSpacing"] : ["dashLength", "gapLength"]
                ColumnLayout {
                    required property string modelData
                    Text { text: modelData === "dotSpacing" ? "Pontos (mm)" : modelData === "dashLength" ? "Traço (mm)" : "Intervalo (mm)"; color: Theme.secondary; font.pixelSize: Theme.caption }
                    C.Field { Layout.fillWidth: true; text: root.canvas ? root.canvas[modelData].toFixed(1) : "2"; validator: DoubleValidator { bottom: 0.1; top: 100; locale: "en_US" } onTextEdited: if(acceptableInput && root.canvas) root.canvas[modelData] = Number(text) }
                }
            }
        }
        Text { visible: !root.marker; text: "Curva de pressão"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.ModernSlider { visible: !root.marker; Layout.fillWidth: true; from: 0.3; to: 3; value: root.canvas ? root.canvas.pressureGamma : 1.2; onMoved: { if(root.canvas) root.canvas.pressureGamma = value } }
        Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: root.marker ? "Ctrl + clique para preencher entre traços. Pequenas aberturas de até 2 mm são toleradas." : "Pressão da stylus · mouse com largura constante"; color: Theme.secondary; font.pixelSize: Theme.caption }
    }
}
}
