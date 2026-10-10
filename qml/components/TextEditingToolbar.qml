import QtQuick
import QtQuick.Controls
import "../theme"
Rectangle {
    id: root
    property bool bold: false
    property bool italic: false
    property bool showColors: true
    property color selectedColor: Theme.text
    property string formatButtonName: "textFormatButton"
    signal formatRequested()
    signal boldRequested()
    signal italicRequested()
    signal colorPicked(color value)
    width: controls.width+8; height: controls.height+8
    radius: Theme.radiusMedium; color: Theme.surface
    border.color: Theme.border; border.width: 1
    Row {
        id: controls; x: 4; y: 4; spacing: 2
        IconButton {
            objectName: root.formatButtonName
            implicitWidth: 32; implicitHeight: 32
            iconName: "settings"; label: "Fonte, tamanho e cor"
            focusPolicy: Qt.NoFocus
            background: Rectangle { radius: 8; color: parent.down ? Theme.pressed : parent.hovered ? Theme.hover : "transparent" }
            onClicked: root.formatRequested()
        }
        Repeater {
            model: root.showColors ? ["B","I"] : []
            ActionButton {
                required property string modelData
                objectName: modelData==="B" ? "inlineBoldButton" : "inlineItalicButton"
                text: modelData; implicitWidth: 32; implicitHeight: 32
                checked: modelData==="B" ? root.bold : root.italic
                focusPolicy: Qt.NoFocus
                Accessible.name: modelData==="B" ? "Negrito" : "Itálico"
                tooltip: modelData==="B" ? "Negrito (Ctrl+B)" : "Itálico (Ctrl+I)"
                background: Rectangle { radius: 8; color: parent.down ? Theme.pressed : parent.checked ? Theme.accentSoft : parent.hovered ? Theme.hover : "transparent" }
                contentItem: Text { text: parent.text; font.family: "Segoe UI"; font.pixelSize: 16; font.bold: parent.text==="B"; font.italic: parent.text==="I"; color: parent.checked ? Theme.accent : Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                onClicked: modelData==="B" ? root.boldRequested() : root.italicRequested()
            }
        }
        Rectangle { visible: root.showColors; width: visible ? 1 : 0; height: 20; y: 6; color: Theme.border }
        Repeater {
            model: root.showColors ? [App.pageColor.hslLightness<0.5 ? Theme.inkNeutralOnDark : Theme.inkNeutralOnLight].concat(Theme.inkHues.map((h,i) => Qt.hsla(h,Theme.inkSaturations[i],App.pageColor.hslLightness<0.5 ? Theme.inkLightnessOnDark[i] : Theme.inkLightnessOnLight[i],1))) : []
            ColorButton {
                required property var modelData
                required property int index
                objectName: "inlineTextColor_"+index
                implicitWidth: 32; implicitHeight: 32
                swatch: modelData; selected: root.selectedColor.toString()===swatch.toString()
                focusPolicy: Qt.NoFocus
                onClicked: root.colorPicked(swatch)
                ToolTip.visible: hovered; ToolTip.text: "Cor do texto"; ToolTip.delay: Theme.tooltipDelay
            }
        }
    }
}
