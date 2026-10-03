import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "newProjectDialog"
    onOpened: { nameField.text = ""; preset.currentIndex = App.defaultSize === "Carta" ? 1 : 0; orientation.currentIndex = App.defaultLandscape ? 1 : 0; background.currentIndex = background.model.indexOf(App.defaultBackground); nameField.forceActiveFocus() }
    contentItem: Flickable {
        implicitHeight: form.implicitHeight
        contentHeight: form.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
        id: form
        width: parent.width
        spacing: Theme.lg
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: Theme.xs
                Text { text: "Um espaço para pensar"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
                Text { text: "Seu próximo quadro começa aqui."; color: Theme.secondary; font.pixelSize: Theme.body }
            }
            Item { Layout.fillWidth: true }
            C.IconButton { iconName: "close"; label: "Fechar"; onClicked: root.close() }
        }
        Text { text: "Nome do projeto"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.Field { id: nameField; objectName: "projectNameInput"; Layout.fillWidth: true; placeholderText: "Ex.: Aula de geometria"; maximumLength: 120 }
        Text { text: "Tamanho físico da página"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.SelectField { id: preset; Layout.fillWidth: true; model: ["A4", "Carta", "Personalizado"] }
        RowLayout {
            visible: preset.currentIndex === 2
            Layout.fillWidth: true
            spacing: Theme.md
            C.Field { id: pageWidth; Layout.fillWidth: true; text: "210"; placeholderText: "Largura (mm)"; validator: DoubleValidator { bottom: 10; top: 5000; locale: "C"; decimals: 2 } }
            Text { text: "×"; color: Theme.secondary }
            C.Field { id: pageHeight; Layout.fillWidth: true; text: "297"; placeholderText: "Altura (mm)"; validator: DoubleValidator { bottom: 10; top: 5000; locale: "C"; decimals: 2 } }
            Text { text: "mm"; color: Theme.secondary; font.pixelSize: Theme.caption }
        }
        C.SegmentedControl { id: orientation; Layout.fillWidth: true; options: ["Retrato", "Paisagem"]; onSelected: index => currentIndex = index }
        Text { text: "Fundo da página"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.SelectField { id: background; Layout.fillWidth: true; model: ["Branco", "Preto", "Verde"] }
        Text {
            Layout.fillWidth: true
            text: preset.currentIndex === 0 ? "A4 · 210 × 297 mm" : preset.currentIndex === 1 ? "Carta · 215,9 × 279,4 mm" : "Dimensões em milímetros · mínimo 10, máximo 5000"
            color: Theme.secondary; font.pixelSize: Theme.caption; wrapMode: Text.WordWrap
        }
        C.ActionButton {
            objectName: "createProjectButton"; text: "Criar quadro"; primary: true; Layout.fillWidth: true
            enabled: !App.busy && (preset.currentIndex !== 2 || (pageWidth.acceptableInput && pageHeight.acceptableInput))
            onClicked: { App.newProject(nameField.text, preset.currentText, Number(pageWidth.text), Number(pageHeight.text), orientation.currentIndex === 1, background.currentText); if (App.active) root.close() }
        }
    }
    }
}
