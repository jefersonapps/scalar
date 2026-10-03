import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
ModernDialog {
    id: root
    objectName: "customColorDialog"
    property color initialColor: "#ffffff"
    property real hue: 0
    property real saturation: 0
    property real brightness: 1
    property bool committed: false
    readonly property color chosenColor: Qt.hsva(hue,saturation,brightness,1)
    signal chosen(color value)
    signal previewed(color value)
    function load(value) {
        const c = Qt.color(value)
        hue = Math.max(0,c.hsvHue); saturation = c.hsvSaturation; brightness = c.hsvValue
    }
    function choose(value) { initialColor = value; committed = false; load(value); open() }
    function setRgb(channel,value) {
        const c = chosenColor
        load(Qt.rgba(channel === 0 ? value/255 : c.r,channel === 1 ? value/255 : c.g,channel === 2 ? value/255 : c.b,1))
    }
    onChosenColorChanged: if(visible) previewed(chosenColor)
    onClosed: if(!committed) chosen(initialColor)
    contentItem: ColumnLayout {
        spacing: Theme.sm
        Text { text: "Cor personalizada"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
        Rectangle {
            id: plane
            objectName: "colorSaturationPlane"
            Layout.fillWidth: true; Layout.preferredHeight: Theme.colorPlaneHeight
            color: Qt.hsva(root.hue,1,1,1); radius: Theme.radiusSmall; clip: true
            Rectangle { anchors.fill: parent; gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#ffffff" } GradientStop { position: 1; color: "#00ffffff" } } }
            Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0; color: "#00000000" } GradientStop { position: 1; color: "#000000" } } }
            Rectangle { x: root.saturation*plane.width-width/2; y: (1-root.brightness)*plane.height-height/2; width: Theme.sliderHandle; height: width; radius: width/2; color: "transparent"; border.width: Theme.focusBorder; border.color: "white"; Rectangle { anchors.fill: parent; anchors.margins: Theme.hairline*3; color: "transparent"; radius: width/2; border.color: "#18181b" } }
            MouseArea {
                anchors.fill: parent
                function pick(mouse) { root.saturation = Math.max(0,Math.min(1,mouse.x/width)); root.brightness = Math.max(0,Math.min(1,1-mouse.y/height)) }
                onPressed: mouse => { plane.forceActiveFocus(); pick(mouse) }
                onPositionChanged: mouse => { if(pressed) pick(mouse) }
            }
            activeFocusOnTab: true
            Accessible.name: "Saturação horizontal e luminosidade vertical. Use as setas para ajustar."
            Keys.onLeftPressed: root.saturation = Math.max(0,root.saturation-0.01)
            Keys.onRightPressed: root.saturation = Math.min(1,root.saturation+0.01)
            Keys.onUpPressed: root.brightness = Math.min(1,root.brightness+0.01)
            Keys.onDownPressed: root.brightness = Math.max(0,root.brightness-0.01)
        }
        Text { text: "Matiz"; color: Theme.secondary; font.pixelSize: Theme.caption }
        Rectangle {
            id: hueBar
            objectName: "colorHueBar"
            Layout.fillWidth: true; Layout.preferredHeight: Theme.colorHueHeight; radius: Theme.radiusSmall
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "#ff0000" }
                GradientStop { position: 1/6; color: "#ffff00" }
                GradientStop { position: 2/6; color: "#00ff00" }
                GradientStop { position: 3/6; color: "#00ffff" }
                GradientStop { position: 4/6; color: "#0000ff" }
                GradientStop { position: 5/6; color: "#ff00ff" }
                GradientStop { position: 1; color: "#ff0000" }
            }
            Rectangle { x: root.hue*(hueBar.width-width); width: Theme.sm; height: parent.height; color: "transparent"; radius: Theme.radiusSmall; border.color: "white"; border.width: Theme.focusBorder }
            MouseArea { anchors.fill: parent; onPressed: mouse => { hueBar.forceActiveFocus(); root.hue = Math.max(0,Math.min(1,mouse.x/width)) }; onPositionChanged: mouse => { if(pressed) root.hue = Math.max(0,Math.min(1,mouse.x/width)) } }
            activeFocusOnTab: true; Accessible.name: "Matiz. Use as setas para ajustar."
            Keys.onLeftPressed: root.hue = Math.max(0,root.hue-0.01)
            Keys.onRightPressed: root.hue = Math.min(1,root.hue+0.01)
        }
        ColorPicker { Layout.fillWidth: true; colors: Theme.basicColors; selectedColor: root.chosenColor; onPicked: value => root.load(value) }
        RowLayout {
            Layout.fillWidth: true
            Rectangle { Layout.preferredWidth: Theme.section; Layout.preferredHeight: Theme.touch; color: root.initialColor; radius: Theme.radiusSmall; border.color: Theme.border; Accessible.name: "Cor anterior" }
            Rectangle { Layout.preferredWidth: Theme.section; Layout.preferredHeight: Theme.touch; color: root.chosenColor; radius: Theme.radiusSmall; border.color: Theme.border; Accessible.name: "Nova cor" }
            Field { objectName: "colorHexField"; Layout.fillWidth: true; text: root.chosenColor.toString(); validator: RegularExpressionValidator { regularExpression: /#[0-9a-fA-F]{6}/ } onEditingFinished: if(acceptableInput) root.load(text); Accessible.name: "Cor hexadecimal" }
        }
        RowLayout {
            Repeater {
                model: ["R","G","B"]
                ColumnLayout {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    Text { text: modelData; color: Theme.secondary; font.pixelSize: Theme.caption }
                    Field { objectName: "colorRgb_" + index; Layout.fillWidth: true; text: Math.round([root.chosenColor.r,root.chosenColor.g,root.chosenColor.b][index]*255); validator: IntValidator { bottom: 0; top: 255 } onEditingFinished: if(acceptableInput) root.setRgb(index,Number(text)); Accessible.name: "Canal " + modelData }
                }
            }
        }
        RowLayout {
            ActionButton { text: "Cancelar"; Layout.fillWidth: true; onClicked: root.close() }
            ActionButton { objectName: "confirmCustomColor"; text: "Usar cor"; primary: true; Layout.fillWidth: true; onClicked: { root.committed = true; root.chosen(root.chosenColor); root.close() } }
        }
    }
}
