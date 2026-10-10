import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"

RowLayout {
    id: root
    property var canvas
    property bool forSelection: false
    readonly property int pattern: !canvas ? 0 : forSelection ? canvas.selectedPattern : ["solid", "dashed", "dotted"].indexOf(canvas.shapeLineStyle)
    visible: pattern > 0
    spacing: Theme.sm
    Repeater {
        model: ["dashLength", "gapLength", "dotSpacing"]
        ColumnLayout {
            id: setting
            required property string modelData
            readonly property string settingProperty: root.forSelection ? "selected" + modelData.charAt(0).toUpperCase() + modelData.slice(1) : modelData
            visible: root.pattern === 2 ? modelData === "dotSpacing" : modelData !== "dotSpacing"
            Layout.fillWidth: true
            Text { text: setting.modelData === "dashLength" ? "Traço (mm)" : setting.modelData === "gapLength" ? "Intervalo (mm)" : "Distância entre pontos (mm)"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            C.Field {
                objectName: (root.forSelection ? "selected" : "shape") + setting.modelData + "Field"
                Layout.fillWidth: true
                text: root.canvas ? root.canvas[setting.settingProperty].toFixed(2) : ""
                // Avoid locale-dependent numeric fixup, which can strip the
                // decimal separator before editingFinished is emitted.
                validator: RegularExpressionValidator { regularExpression: /^(?:\d+(?:[.,]\d*)?|[.,]\d+)$/ }
                onEditingFinished: {
                    const value = Number(text.replace(",", "."))
                    if (root.canvas && acceptableInput && Number.isFinite(value) && value >= 0.1 && value <= 100)
                        root.canvas[setting.settingProperty] = value
                    text = Qt.binding(() => root.canvas ? root.canvas[setting.settingProperty].toFixed(2) : "")
                }
            }
        }
    }
}
