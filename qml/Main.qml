import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "pages"
import "dialogs"
import "components" as C
import "theme"
ApplicationWindow {
    id: window
    width: Theme.windowWidth; height: Theme.windowHeight
    minimumWidth: Theme.minimumWidth; minimumHeight: Theme.minimumHeight
    visible: true
    title: App.active ? App.projectName + " — Scalar" : "Scalar"
    color: Theme.background
    onClosing: close => { close.accepted = App.shutdown() }
    Loader {
        id: pageLoader
        anchors.fill: parent
        sourceComponent: App.active ? editor : home
    }
    Component {
        id: home
        HomePage { onCreateRequested: newProject.open(); onOpenRequested: openFile.open(); onImportPdfRequested: pdfFile.open(); onSettingsRequested: settings.open() }
    }
    Component {
        id: editor
        EditorPage { overlaysOpen: settings.visible || saveFile.visible || openFile.visible || imageFile.visible || pdfFile.visible || exportFile.visible || pdfImport.visible || newProject.visible || recovery.visible; onSaveAsRequested: saveFile.open(); onImageRequested: imageFile.open(); onImportPdfRequested: pdfFile.open(); onExportPdfRequested: exportFile.open(); onSettingsRequested: settings.open() }
    }
    NewProjectDialog { id: newProject }
    SettingsDialog { id: settings }
    ImportPdfDialog { id: pdfImport }
    FileDialog { id: pdfFile; title: "Importar PDF"; nameFilters: ["PDF (*.pdf)"]; onAccepted: App.inspectPdfFile(selectedFile) }
    FileDialog { id: openFile; title: "Abrir quadro"; nameFilters: ["Projetos Scalar (*.board)"]; onAccepted: App.open(selectedFile) }
    FileDialog { id: saveFile; title: "Salvar quadro como"; fileMode: FileDialog.SaveFile; defaultSuffix: "board"; nameFilters: ["Projetos Scalar (*.board)"]; onAccepted: App.saveAs(selectedFile) }
    FileDialog { id: exportFile; objectName: "exportPdfFileDialog"; title: "Exportar todas as páginas como PDF"; fileMode: FileDialog.SaveFile; defaultSuffix: "pdf"; nameFilters: ["PDF (*.pdf)"]; onAccepted: App.exportPdf(selectedFile) }
    FileDialog { id: imageFile; title: "Importar imagens"; fileMode: FileDialog.OpenFiles; nameFilters: ["Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.svg)"]; onAccepted: { if(pageLoader.item) pageLoader.item.insertImages(selectedFiles) } }
    C.ModernDialog {
        id: recovery
        closePolicy: Popup.NoAutoClose
        contentItem: ColumnLayout {
            spacing: Theme.lg
            Text { text: "Suas ideias continuam aqui"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Text { text: "Encontramos alterações de uma sessão não finalizada. Deseja recuperar o último quadro?"; color: Theme.secondary; font.pixelSize: Theme.body; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            C.ActionButton { text: "Recuperar quadro"; primary: true; Layout.fillWidth: true; onClicked: { App.recover(); recovery.close() } }
            C.ActionButton { text: "Descartar recuperação"; Layout.fillWidth: true; onClicked: { App.discardRecovery(); recovery.close() } }
        }
    }
    Component.onCompleted: { if(App.recoveryAvailable) recovery.open() }
    Shortcut { sequence: "Ctrl+O"; enabled: !settings.visible && !newProject.visible && !recovery.visible; onActivated: openFile.open() }
    Shortcut { sequence: "Ctrl+N"; enabled: !settings.visible && !newProject.visible && !recovery.visible; onActivated: newProject.open() }
}
