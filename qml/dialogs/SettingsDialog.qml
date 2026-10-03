import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "settingsDialog"
    padding: Theme.lg
    contentItem: Flickable {
        implicitHeight: form.implicitHeight
        contentHeight: form.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
            id: form
            width: parent.width
            spacing: Theme.md
            RowLayout {
                Text { text: "Preferências"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold; Layout.fillWidth: true }
                C.IconButton { compact: true; iconName: "close"; label: "Fechar configurações"; onClicked: root.close() }
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: Theme.sm
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Aparência"; color: Theme.text; font.pixelSize: Theme.body; Layout.fillWidth: true }
                    C.SegmentedControl {
                        Layout.preferredWidth: Theme.settingsSegmentWidth
                        options: ["Claro", "Escuro", "Sistema"]
                        currentIndex: App.theme === "Light" ? 0 : App.theme === "Dark" ? 1 : 2
                        onSelected: index => App.theme = ["Light", "Dark", "System"][index]
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredHeight: Theme.controlHeight
                    Text { text: "Reduzir efeitos visuais"; color: Theme.secondary; font.pixelSize: Theme.body; Layout.fillWidth: true }
                    Switch {
                        id: effects
                        checked: App.reducedEffects; onToggled: App.reducedEffects = checked
                        Accessible.name: "Reduzir efeitos visuais"
                        implicitWidth: Theme.touch; implicitHeight: Theme.controlHeight
                        indicator: Rectangle {
                            anchors.centerIn: parent; width: Theme.toggleWidth; height: Theme.toggleHeight; radius: height/2
                            color: effects.checked ? Theme.accent : Theme.border
                            Rectangle {
                                x: effects.checked ? parent.width-width-Theme.xs/2 : Theme.xs/2
                                y: Theme.xs/2; width: parent.height-Theme.xs; height: width; radius: width/2
                                color: effects.checked ? Theme.accentText : Theme.text
                                Behavior on x { NumberAnimation { duration: Theme.fast } }
                            }
                        }
                    }
                }
            }
            Rectangle { Layout.fillWidth: true; height: Theme.hairline; color: Theme.border }
            ColumnLayout {
                Layout.fillWidth: true; spacing: Theme.sm
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Reconhecimento"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.Medium; Layout.fillWidth: true }
                    C.SegmentedControl {
                        Layout.preferredWidth: Theme.settingsSegmentWidth
                        options: ["Desativado", "Ao segurar"]; currentIndex: App.recognitionEnabled ? 1 : 0
                        onSelected: index => App.recognitionEnabled = index === 1
                    }
                }
                Text {
                    objectName: "recognitionShiftHint"
                    text: "Desenhe e segure para reconhecer. Segure Shift durante o reconhecimento para uma linha ou contorno tracejado."
                    Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.caption
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Tempo"; color: Theme.secondary; font.pixelSize: Theme.caption }
                    C.ModernSlider { Layout.fillWidth: true; from: 250; to: 1500; stepSize: 50; value: App.holdDelay; onMoved: App.holdDelay = Math.round(value); enabled: App.recognitionEnabled }
                    Text { text: App.holdDelay + " ms"; color: Theme.text; font.pixelSize: Theme.caption; Layout.minimumWidth: Theme.touch }
                }
            }
            Rectangle { Layout.fillWidth: true; height: Theme.hairline; color: Theme.border }
            ColumnLayout {
                Layout.fillWidth: true; spacing: Theme.sm
                Text { text: "Novos projetos"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.Medium }
                RowLayout {
                    Layout.fillWidth: true; spacing: Theme.sm
                    C.SelectField { Layout.fillWidth: true; model: ["A4", "Carta"]; currentIndex: App.defaultSize === "Carta" ? 1 : 0; onActivated: App.defaultSize = currentText; Accessible.name: "Tamanho padrão" }
                    C.SegmentedControl { Layout.preferredWidth: Theme.settingsSegmentWidth; options: ["Retrato", "Paisagem"]; currentIndex: App.defaultLandscape ? 1 : 0; onSelected: index => App.defaultLandscape = index === 1 }
                }
                C.SelectField { Layout.fillWidth: true; model: App.gridPresets.map(p => p.name); currentIndex: model.indexOf(App.defaultBackground); onActivated: App.defaultBackground = currentText; Accessible.name: "Grade padrão" }
                Text { text: "Novos quadros: branco no tema claro e preto no escuro. Quadros salvos preservam sua cor."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.caption }
            }
        }
    }
}
