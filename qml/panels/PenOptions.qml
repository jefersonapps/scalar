import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPopover {
    id: root
    objectName: "penOptions"
    property var canvas
    contentItem: ColumnLayout {
        spacing: Theme.md
        Text { text: "Sua caneta"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
        C.ColorPicker { selectedColor: root.canvas ? root.canvas.penColor : "#263345"; onPicked: value => { if(root.canvas) root.canvas.penColor = value } }
        RowLayout {
            Text { text: "Espessura máxima"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true }
            Text { text: root.canvas ? root.canvas.penWidth.toFixed(2) + " mm" : ""; color: Theme.text; font.pixelSize: Theme.caption }
        }
        C.ModernSlider { Layout.fillWidth: true; from: 0.2; to: 3; value: root.canvas ? root.canvas.penWidth : 0.85; onMoved: { if(root.canvas) root.canvas.penWidth = value } }
        Text { text: "Curva de pressão"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.ModernSlider { Layout.fillWidth: true; from: 0.3; to: 3; value: root.canvas ? root.canvas.pressureGamma : 1.2; onMoved: { if(root.canvas) root.canvas.pressureGamma = value } }
        Text { text: "Pressão da stylus · mouse com largura constante"; color: Theme.secondary; font.pixelSize: Theme.caption }
    }
}
