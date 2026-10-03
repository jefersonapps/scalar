import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "settingsDialog"
    contentItem: ColumnLayout {
        spacing: Theme.lg
        RowLayout {
            Text { text: "Do seu jeito"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold; Layout.fillWidth: true }
            C.IconButton { iconName: "close"; label: "Fechar configurações"; onClicked: root.close() }
        }
        Text { text: "Aparência da interface"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.SegmentedControl {
            Layout.fillWidth: true; options: ["Claro", "Escuro", "Sistema"]
            currentIndex: App.theme === "Light" ? 0 : App.theme === "Dark" ? 1 : 2
            onSelected: index => App.theme = ["Light", "Dark", "System"][index]
        }
        CheckBox {
            id: effects
            text: "Reduzir efeitos visuais"; checked: App.reducedEffects
            onToggled: App.reducedEffects = checked
            font.pixelSize: Theme.body
            contentItem: Text { text: effects.text; color: Theme.text; font: effects.font; leftPadding: effects.indicator.width+Theme.sm; verticalAlignment: Text.AlignVCenter }
            indicator: Rectangle { implicitWidth: Theme.xl; implicitHeight: Theme.xl; x: effects.leftPadding; y: (effects.height-height)/2; radius: Theme.radiusSmall/2; color: effects.checked ? Theme.accent : Theme.background; border.color: Theme.border; C.Icon { anchors.centerIn: parent; width: Theme.lg; height: Theme.lg; name: "check"; color: Theme.accentText; visible: effects.checked } }
        }
        Text { text: "Padrão para novos projetos"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.SelectField { Layout.fillWidth: true; model: ["A4", "Carta"]; currentIndex: App.defaultSize === "Carta" ? 1 : 0; onActivated: App.defaultSize = currentText }
        C.SegmentedControl { Layout.fillWidth: true; options: ["Retrato", "Paisagem"]; currentIndex: App.defaultLandscape ? 1 : 0; onSelected: index => App.defaultLandscape = index === 1 }
        C.SelectField { Layout.fillWidth: true; model: ["Branco", "Preto", "Verde"]; currentIndex: model.indexOf(App.defaultBackground); onActivated: App.defaultBackground = currentText }
        Text { text: "O tema da interface preserva o fundo do seu quadro."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.caption }
    }
}
