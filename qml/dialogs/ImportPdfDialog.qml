import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "importPdfDialog"
    closePolicy: App.pdfBusy ? Popup.NoAutoClose : Popup.CloseOnEscape
    onOpened: { mode.currentIndex = 0; range.text = "" }
    onClosed: App.cancelPdfImport()
    Connections {
        target: App
        function onPdfImportRequested() { root.open() }
        function onPdfImported() { root.close() }
    }
    contentItem: ColumnLayout {
        spacing: Theme.md
        Text { text: "Importar PDF"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
        Text { text: App.pdfBusy ? "Lendo documento…" : App.pdfName + " · " + App.pdfPageCount + " páginas"; Layout.fillWidth: true; elide: Text.ElideMiddle; color: Theme.secondary; font.pixelSize: Theme.body }
        C.SegmentedControl { id: mode; Layout.fillWidth: true; options: ["Todas as páginas","Páginas específicas"]; enabled: !App.pdfBusy; onSelected: index => currentIndex = index }
        C.Field { id: range; objectName: "pdfPageRange"; Layout.fillWidth: true; visible: mode.currentIndex === 1; placeholderText: "Ex.: 1, 3-5" }
        Text { text: "Cada página do PDF vira uma página do quadro, preservando suas medidas. As anotações ficam acima do documento."; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.caption }
        Text { text: App.pdfError; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.danger; font.pixelSize: Theme.body }
        RowLayout {
            C.ActionButton { text: "Cancelar"; Layout.fillWidth: true; enabled: !App.pdfBusy; onClicked: root.close() }
            C.ActionButton { objectName: "confirmPdfImport"; text: "Importar"; primary: true; Layout.fillWidth: true; enabled: !App.pdfBusy && App.pdfPageCount > 0 && (mode.currentIndex === 0 || range.text.trim().length > 0); onClicked: App.importPdfPages(mode.currentIndex === 0 ? "" : range.text) }
        }
    }
}
