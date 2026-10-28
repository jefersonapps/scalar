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
    property string homeFolder: ""
    property bool wasMaximized: false
    property bool shutdownApproved: false
    function toggleFullScreen() {
        if(visibility===Window.FullScreen){if(wasMaximized)showMaximized();else showNormal()}
        else {wasMaximized=visibility===Window.Maximized;showFullScreen()}
    }
    width: Theme.windowWidth; height: Theme.windowHeight
    minimumWidth: Theme.minimumWidth; minimumHeight: Theme.minimumHeight
    visible: true
    title: App.active ? App.projectName + " — Scalar" : "Scalar"
    color: Theme.background
    onClosing: close => {
        close.accepted=window.shutdownApproved
        if(window.shutdownApproved || App.leaving)return
        if(App.active && pageLoader.item && !pageLoader.item.prepareToClose())return
        App.requestShutdown()
    }
    Connections {
        target: App
        function onShutdownReady() { window.shutdownApproved=true; Qt.callLater(function(){window.close()}) }
    }
    Loader {
        id: pageLoader
        anchors.fill: parent
        enabled: !App.leaving
        sourceComponent: App.active ? editor : home
    }
    Component {
        id: home
        HomePage {
            initialFolder: window.homeFolder
            onCreateRequested: { window.homeFolder=""; newProject.open() }
            onProjectRequested: (path,folderId) => { window.homeFolder=folderId; App.openPath(path) }
            onFolderProjectCreated: folderId => window.homeFolder=folderId
            onOpenRequested: { window.homeFolder=""; openFile.open() }
            onImportPdfRequested: pdfFile.open()
            onSettingsRequested: settings.open()
            onFullScreenRequested: window.toggleFullScreen()
        }
    }
    DropArea {
        id: projectDrop
        objectName: "projectImportDropArea"
        parent: Overlay.overlay
        anchors.fill: parent
        z: 10000
        enabled: !App.leaving && !App.loading
        property Item targetFolder: null
        function clearTarget() {
            if(targetFolder)targetFolder.importDropActive=false
            targetFolder=null
        }
        function updateTarget(event) {
            const point=pageLoader.item ? mapToItem(pageLoader.item,event.x,event.y) : Qt.point(0,0)
            const target=!App.active && pageLoader.item ? pageLoader.item.importFolderAt(point.x,point.y) : null
            if(target===targetFolder)return
            clearTarget();targetFolder=target
            if(targetFolder)targetFolder.importDropActive=true
        }
        onEntered: drag => {
            drag.accepted=drag.hasUrls && drag.urls.length===1 && App.canImportProject(drag.urls[0])
            if(drag.accepted){drag.accept(Qt.CopyAction);updateTarget(drag)}
        }
        onPositionChanged: drag => updateTarget(drag)
        onExited: clearTarget()
        onEnabledChanged: if(!enabled)clearTarget()
        onDropped: drop => {
            if(!drop.hasUrls || drop.urls.length!==1 || !App.canImportProject(drop.urls[0]))return
            updateTarget(drop)
            const url=drop.urls[0]
            const folder=targetFolder ? targetFolder.importFolderId : pageLoader.item && !App.active ? pageLoader.item.activeFolder : ""
            clearTarget()
            drop.accept(Qt.CopyAction)
            if(App.active && pageLoader.item)pageLoader.item.importDroppedProject(url)
            else {
                window.homeFolder=pageLoader.item ? pageLoader.item.activeFolder : ""
                App.requestImportProject(url,folder)
            }
        }
    }
    Component {
        id: editor
        EditorPage { overlaysOpen: App.leaving || settings.visible || saveFile.visible || openFile.visible || imageFile.visible || pdfFile.visible || exportFile.visible || pdfImport.visible || newProject.visible || recovery.visible; onBackRequested: App.requestHome(); onSaveAsRequested: saveFile.open(); onImportBoardRequested: openFile.open(); onImageRequested: imageFile.open(); onImportPdfRequested: pdfFile.open(); onExportPdfRequested: exportFile.openForProject(); onSettingsRequested: settings.open();onFullScreenRequested: window.toggleFullScreen() }
    }
    NewProjectDialog { id: newProject }
    SettingsDialog { id: settings }
    ImportPdfDialog { id: pdfImport }
    FileDialog { id: pdfFile; title: "Importar PDF"; nameFilters: ["PDF (*.pdf)"]; onAccepted: App.inspectPdfFile(selectedFile) }
    FileDialog { id: openFile; objectName: "importBoardFileDialog"; title: "Importar quadro"; nameFilters: ["Quadros Scalar (*.scalar *.board *.zip)"]; onAccepted: { window.homeFolder=""; App.open(selectedFile) } }
    FileDialog { id: saveFile; objectName: "saveBoardFileDialog"; title: "Salvar quadro completo como"; fileMode: FileDialog.SaveFile; defaultSuffix: "scalar"; nameFilters: ["Quadro Scalar (*.scalar)"]; onAccepted: App.saveAs(selectedFile) }
    FileDialog {
        id: exportFile; objectName: "exportPdfFileDialog"
        title: "Exportar todas as páginas como PDF"
        fileMode: FileDialog.SaveFile; defaultSuffix: "pdf"; nameFilters: ["PDF (*.pdf)"]
        function openForProject() { selectedFile=App.suggestedPdfExportFile(currentFolder);open() }
        onAccepted: App.exportPdf(selectedFile)
    }
    FileDialog { id: imageFile; title: "Importar imagens"; fileMode: FileDialog.OpenFiles; nameFilters: ["Imagens (*.png *.jpg *.jpeg *.webp *.bmp *.svg)"]; onAccepted: { if(pageLoader.item) pageLoader.item.insertImages(selectedFiles) } }
    C.ModernDialog {
        id: savingDialog
        objectName: "savingBeforeLeavingDialog"
        visible: App.leaving
        closePolicy: Popup.NoAutoClose
        contentItem: ColumnLayout {
            spacing: Theme.lg
            BusyIndicator { objectName: "savingBeforeLeavingSpinner"; Layout.alignment: Qt.AlignHCenter; running: App.leaving && App.leaveError===""; visible: running }
            Text { Layout.fillWidth: true; text: App.leaveError ? "Não foi possível salvar" : "Salvando…"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
            Text { Layout.fillWidth: true; text: App.leaveError || "Aguarde enquanto suas alterações são salvas."; color: Theme.secondary; font.pixelSize: Theme.body; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
            C.ActionButton { objectName: "retryLeavingSaveButton"; visible: App.leaveError!==""; Layout.fillWidth: true; text: "Tentar novamente"; primary: true; onClicked: App.retryLeaving() }
            C.ActionButton { objectName: "cancelLeavingSaveButton"; visible: App.leaveError!==""; Layout.fillWidth: true; text: "Continuar editando"; onClicked: App.cancelLeaving() }
        }
    }
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
    Component.onCompleted: {
        App.detectToolbarBlurSupport(window)
        if(App.recoveryAvailable) recovery.open()
    }
    Shortcut { sequence: "Ctrl+O"; enabled: !App.leaving && !settings.visible && !newProject.visible && !recovery.visible; onActivated: openFile.open() }
    Shortcut { sequence: "Ctrl+N"; enabled: !App.leaving && !settings.visible && !newProject.visible && !recovery.visible; onActivated: newProject.open() }
    Shortcut { sequence: "F11";onActivated: window.toggleFullScreen() }
}
