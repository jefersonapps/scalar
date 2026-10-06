import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Scalar 1.0
import "../components" as C
import "../theme"
import "../dialogs"
Item {
    id: root
    readonly property bool hasSidebar: width>=Theme.hintBreakpoint
    readonly property bool compactHome: contentArea.width<Theme.mediumBreakpoint || height<Theme.minimumHeight+Theme.section*2
    property string viewMode: "folder"
    readonly property var displayedProjects: { const projects=App.recentProjects; return App.searchProjects(searchField.text,activeFolder,viewMode) }
    property string activeFolder: ""
    property string initialFolder: ""
    readonly property string folderTitle: { const entry=App.folders.find(folder=>folder.id===activeFolder); return entry ? entry.name : "Seus quadros" }
    readonly property string parentFolder: { const entry=App.folders.find(folder=>folder.id===activeFolder); return entry ? entry.parentId : "" }
    readonly property var childFolders: App.folders.filter(folder => folder.parentId===activeFolder)
    signal createRequested()
    signal openRequested()
    signal settingsRequested()
    signal fullScreenRequested()
    readonly property bool fullScreen: root.Window.window && root.Window.window.visibility===Window.FullScreen
    signal importPdfRequested()
    signal projectRequested(string path, string folderId)
    signal folderProjectCreated(string folderId)
    function enterFolder(folderId) { viewMode="folder"; activeFolder=folderId }
    DeleteProjectDialog { id: deletion }
    EditProjectDialog { id: projectEditor; onTrashRequested: project => deletion.ask(project,false) }
    FolderDialog { id: folderDialog; onDeleteRequested: folderId => { const entry=App.folders.find(folder => folder.id===folderId); folderDialog.close(); if(entry)folderDeletion.ask(entry) } }
    DeleteFolderDialog { id: folderDeletion; onDeletionConfirmed: (folderId,parentId) => { if(App.deleteFolder(folderId) && !App.folders.some(folder => folder.id===root.activeFolder))root.activeFolder=parentId } }
    TrashDialog { id: trash; onPermanentDeletionRequested: entry => deletion.ask(entry,true) }
    Component.onCompleted: {
        if(initialFolder!=="" && App.folders.some(folder => folder.id===initialFolder)) activeFolder=initialFolder
    }
    C.LibrarySidebar {
        id: sidebar; width: Theme.librarySidebarWidth; height: parent.height; visible: root.hasSidebar
        activeFolder: root.activeFolder; viewMode: root.viewMode
        onFolderRequested: folderId => { root.activeFolder=folderId; root.viewMode="folder"; searchField.text="" }
        onSectionRequested: mode => { root.activeFolder=""; root.viewMode=mode; searchField.text="" }
    }
    Item {
    id: contentArea
    anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right
    anchors.left: root.hasSidebar ? sidebar.right : parent.left
    ColumnLayout {
        width: Math.min(Theme.homeContentWidth,contentArea.width-(root.compactHome?Theme.lg:Theme.xl)*2)
        height: root.height-(root.width<Theme.mediumBreakpoint?Theme.lg:Theme.xxl)*2
        anchors.centerIn: parent
        spacing: root.compactHome?Theme.md:root.height<Theme.windowHeight+Theme.section*2?Theme.lg:Theme.xl
        Item {
            Layout.fillWidth: true
            implicitHeight: Theme.touch
            RowLayout {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.md
                ApplicationLogo { objectName: "applicationLogo"; visible: !root.hasSidebar; Layout.preferredWidth: Theme.touch; Layout.preferredHeight: Theme.touch }
                Text { text: root.hasSidebar ? "Sua biblioteca" : "Scalar"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
            }
            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.lg
                C.Field {
                    id: searchField; objectName: "boardSearchField"
                    Layout.preferredWidth: Math.min(360,contentArea.width-(root.hasSidebar?380:360))
                    placeholderText: "Buscar quadros"; leftPadding: Theme.icon+Theme.xl; rightPadding: Theme.touch
                    C.Icon { name: "search"; color: Theme.secondary; anchors.left: parent.left; anchors.leftMargin: Theme.md; anchors.verticalCenter: parent.verticalCenter }
                    C.IconButton { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; iconName: "close"; visible: searchField.text!==""; label: "Limpar busca"; onClicked: searchField.clear() }
                }
                C.IconButton { objectName: "openTrashButton"; iconName: "trash"; label: "Lixeira (" + App.trashedProjects.length + ")"; onClicked: trash.open() }
                C.IconButton { objectName: "homeFullScreenButton";iconName: root.fullScreen ? "restoreScreen" : "fullScreen";label: root.fullScreen ? "Sair da tela cheia (F11)" : "Tela cheia (F11)";selected: root.fullScreen;onClicked: root.fullScreenRequested() }
                C.IconButton { iconName: "settings"; label: "Configurações"; onClicked: root.settingsRequested() }
            }
        }
        Rectangle {
            id: heroPanel
            readonly property real contentPadding: root.compactHome ? Theme.lg : Theme.xl
            visible: root.activeFolder==="" && root.viewMode==="folder" && searchField.text==="" && (App.folders.length===0 || root.height>=Theme.windowHeight+Theme.section*2)
            implicitHeight: Math.max(root.compactHome ? Theme.homeHeroCompactHeight : Theme.homeHeroMediumHeight, heroCopy.implicitHeight + contentPadding*2)
            Layout.fillWidth: true; Layout.preferredHeight: implicitHeight; Layout.minimumHeight: implicitHeight
            color: Theme.surface; radius: Theme.radiusFloating; border.color: Theme.border
            RowLayout {
                anchors.fill: parent; anchors.margins: heroPanel.contentPadding; spacing: Theme.xxl
                ColumnLayout {
                    id: heroCopy
                    Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter; spacing: Theme.md
                    Text { visible: !root.compactHome&&root.height>=Theme.windowHeight+Theme.section*2; text: "ESCREVER · ENSINAR · EXPLORAR"; color: Theme.secondary; font.pixelSize: Theme.caption; font.letterSpacing: 1 }
                    Text { Layout.fillWidth: true; text: "Dê espaço às\nsuas próximas ideias."; color: Theme.text; font.pixelSize: root.compactHome ? Theme.heading : root.height<Theme.windowHeight+Theme.section*2 ? Theme.heading+4 : Theme.title; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
                    Text { visible: !root.compactHome; Layout.fillWidth: true; text: "Da primeira anotação à próxima aula. Tudo no seu quadro."; color: Theme.secondary; font.pixelSize: Theme.body; wrapMode: Text.WordWrap }
                    C.ActionButton { objectName: "newProjectButton"; text: "Criar novo quadro"; primary: true; onClicked: root.createRequested() }
                }
                Item {
                    Layout.preferredWidth: Theme.cardWidth; Layout.fillHeight: true; visible: contentArea.width > Theme.hintBreakpoint
                    Rectangle {
                        anchors.fill: parent; radius: Theme.radiusLarge; color: Theme.workspace; border.color: Theme.border; rotation: -3
                        Shape {
                            id: heroArt
                            anchors.fill: parent
                            ShapePath { strokeColor: Theme.secondary; strokeWidth: Theme.focusBorder; fillColor: "transparent"; PathAngleArc { centerX: heroArt.width*.32; centerY: heroArt.height*.45; radiusX: heroArt.width*.17; radiusY: radiusX; startAngle: 0; sweepAngle: 360 } }
                            ShapePath { strokeColor: Theme.accent; strokeWidth: Theme.focusBorder; fillColor: Theme.accentSoft; startX: heroArt.width*.62; startY: heroArt.height*.2; PathLine { x: heroArt.width*.85; y: heroArt.height*.67 } PathLine { x: heroArt.width*.52; y: heroArt.height*.67 } PathLine { x: heroArt.width*.62; y: heroArt.height*.2 } }
                        }
                        Text { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: Theme.lg; text: "Ideias tomam forma."; color: Theme.secondary; font.pixelSize: Theme.caption }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: Theme.md
            visible: root.activeFolder==="" && root.viewMode==="folder" && searchField.text==="" && (!root.compactHome || App.folders.length===0)
            Repeater {
                model: [{icon:"pen",title:"Começar agora",detail:"Um quadro com seu padrão",action:"quick"},{icon:"folder",title:"Abrir projeto",detail:"Retome um arquivo .board",action:"open"},{icon:"page",title:"Importar PDF",detail:"Anote sobre suas páginas",action:"pdf"}]
                AbstractButton {
                    required property var modelData
                    Layout.fillWidth: true; Layout.preferredHeight: root.height<Theme.windowHeight+Theme.section*2 ? Theme.touch+Theme.lg : Theme.homeActionHeight
                    hoverEnabled: true; enabled: !App.busy
                    Accessible.name: modelData.title
                    background: Rectangle { radius: Theme.radiusLarge; color: parent.hovered ? Theme.hover : Theme.surface; border.color: Theme.border }
                    contentItem: RowLayout {
                        spacing: Theme.md
                        C.Icon { name: modelData.icon; color: Theme.secondary; visible: root.width > Theme.compactBreakpoint }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.xs
                            Text { Layout.fillWidth: true; text: modelData.title; wrapMode: Text.WordWrap; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
                            Text { Layout.fillWidth: true; visible: root.width > Theme.compactBreakpoint; text: modelData.detail; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.caption }
                        }
                    }
                    padding: Theme.lg
                    onClicked: { if(modelData.action==="quick") App.newDefault(); else if(modelData.action==="open") root.openRequested(); else root.importPdfRequested() }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.lg
            C.IconButton {
                objectName: "backToProjectsButton"
                visible: root.activeFolder!==""; iconName: "back"
                label: root.parentFolder ? "Voltar para a pasta anterior" : "Voltar aos seus quadros"
                onClicked: root.activeFolder=root.parentFolder
                DropArea {
                    anchors.fill: parent; keys: ["scalar-project"]
                    onDropped: event => {
                        if(!event.source || !event.source.projectId)return
                        const project=event.source.projectId
                        event.accept()
                        Qt.callLater(function(){ if(App.moveProjectToFolder(project,root.parentFolder))root.activeFolder=root.parentFolder })
                    }
                }
            }
            Text { Layout.fillWidth: true; text: searchField.text ? "Resultados da busca" : root.viewMode==="recent" ? "Quadros recentes" : root.viewMode==="all" ? "Todos os quadros" : root.folderTitle; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
            C.IconButton { visible: root.activeFolder!==""; iconName: "settings"; label: "Editar pasta"; onClicked: folderDialog.edit(App.folders.find(folder=>folder.id===root.activeFolder)) }
            Item { Layout.fillWidth: true; implicitHeight: 1 }
            Text { text: root.displayedProjects.length + (root.displayedProjects.length===1 ? " quadro" : " quadros"); visible: root.width > Theme.compactBreakpoint; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.ActionButton {
                objectName: "newProjectInFolderButton"
                visible: root.activeFolder!==""
                text: "Novo quadro"
                iconName: "pen"
                primary: true
                onClicked: { root.folderProjectCreated(root.activeFolder); App.newDefaultInFolder(root.activeFolder) }
            }
            C.ActionButton { objectName: "newFolderButton"; text: "Nova pasta"; iconName: "folder"; onClicked: folderDialog.create(root.activeFolder) }
        }
        Flickable {
            Layout.fillWidth: true; Layout.preferredHeight: Theme.folderCardHeight
            visible: root.childFolders.length>0 && root.viewMode==="folder" && searchField.text===""; clip: true
            contentWidth: foldersRow.implicitWidth; contentHeight: height
            boundsBehavior: Flickable.StopAtBounds
            Row {
                id: foldersRow; spacing: Theme.md
                Repeater {
                    model: root.childFolders
                    C.FolderCard {
                        required property var modelData
                        folderId: modelData.id; folderName: modelData.name; folderColor: modelData.color; projectCount: modelData.count
                        onClicked: root.enterFolder(folderId)
                        onEditRequested: folderDialog.edit(modelData)
                    }
                }
            }
        }
        GridView {
            id: grid
            readonly property real tileHeight: Math.min(Theme.cardHeight,Math.max(Theme.colorPlaneHeight,height))
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            cellWidth: width/Math.max(1,Math.floor(width/Theme.cardWidth))
            cellHeight: tileHeight+Theme.lg
            model: root.displayedProjects
            delegate: C.ProjectCard {
                required property var modelData
                width: grid.cellWidth-Theme.lg
                height: grid.tileHeight
                projectName: modelData.name
                projectId: modelData.id
                updated: modelData.updated
                thumbnail: modelData.thumbnail
                onClicked: root.projectRequested(modelData.path,modelData.folderId)
                onEditRequested: projectEditor.edit(modelData)
            }
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.md
                visible: grid.count === 0
                C.Icon { Layout.alignment: Qt.AlignHCenter; name: "page"; color: Theme.secondary; width: Theme.xxl; height: Theme.xxl }
                Text { text: searchField.text ? "Nenhum quadro encontrado." : root.activeFolder ? "Crie um quadro ou arraste um para esta pasta." : root.viewMode!=="folder" ? "Seus quadros aparecerão aqui." : App.folders.length ? "Seus quadros estão nas pastas acima." : "Seu primeiro quadro está esperando."; color: Theme.secondary; font.pixelSize: Theme.body; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            }
        }
        Text { Layout.fillWidth: true; text: App.status; color: Theme.secondary; font.pixelSize: Theme.caption; elide: Text.ElideRight }
    }
    }
}
