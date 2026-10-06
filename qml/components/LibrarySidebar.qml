import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Scalar 1.0
import "../theme"
Rectangle {
    id: root
    property string activeFolder: ""
    property string viewMode: "folder"
    property var expanded: ({})
    signal sectionRequested(string mode)
    signal folderRequested(string folderId)
    color: Theme.surface
    readonly property var treeRows: {
        const folders=App.folders, rows=[], stack=folders.filter(folder=>folder.parentId==="").reverse().map(folder=>({folder:folder,depth:0}))
        const visited={}
        while(stack.length){
            const item=stack.pop(),folder=item.folder
            if(visited[folder.id])continue
            visited[folder.id]=true; rows.push({folder:folder,depth:item.depth})
            if(root.expanded[folder.id])folders.filter(child=>child.parentId===folder.id).reverse().forEach(child=>stack.push({folder:child,depth:item.depth+1}))
        }
        return rows
    }
    function revealFolder(folderId) {
        const next=Object.assign({},expanded),visited={}
        let current=App.folders.find(folder=>folder.id===folderId)
        while(current && !visited[current.id]) { visited[current.id]=true; next[current.id]=true; current=App.folders.find(folder=>folder.id===current.parentId) }
        expanded=next
    }
    onActiveFolderChanged: revealFolder(activeFolder)
    ColumnLayout {
        anchors.fill: parent; anchors.margins: Theme.lg; spacing: Theme.md
        RowLayout {
            Layout.fillWidth: true; spacing: Theme.md
            ApplicationLogo { Layout.preferredWidth: Theme.touch; Layout.preferredHeight: Theme.touch }
            Text { text: "Scalar"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
        }
        Item { Layout.preferredHeight: Theme.md }
        Repeater {
            model: [{label:"Início",icon:"home",mode:"folder"},{label:"Recentes",icon:"page",mode:"recent"},{label:"Todos os quadros",icon:"background",mode:"all"}]
            AbstractButton {
                required property var modelData
                Layout.fillWidth: true; Layout.preferredHeight: Theme.touch; hoverEnabled: true
                readonly property bool selected: root.activeFolder===""&&root.viewMode===modelData.mode
                background: Rectangle { radius: Theme.radiusSmall; color: parent.selected ? Theme.accentSoft : parent.hovered ? Theme.hover : "transparent" }
                contentItem: RowLayout {
                    spacing: Theme.md
                    Icon { name: modelData.icon; color: parent.parent.selected ? Theme.text : Theme.secondary }
                    Text { Layout.fillWidth: true; text: modelData.label; color: Theme.text; font.pixelSize: Theme.body }
                }
                padding: Theme.md
                onClicked: root.sectionRequested(modelData.mode)
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: Theme.lg
            Text { text: "PASTAS"; color: Theme.secondary; font.pixelSize: Theme.caption; font.letterSpacing: 1 }
            Item { Layout.fillWidth: true }
        }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: parent.width; spacing: Theme.xs
                Repeater {
                    model: root.treeRows
                    AbstractButton {
                        required property var modelData
                        Layout.fillWidth: true; Layout.preferredHeight: Theme.touch
                        hoverEnabled: true; padding: Theme.sm
                        leftPadding: Theme.sm+Math.min(modelData.depth,4)*Theme.sm
                        Accessible.name: "Abrir pasta "+modelData.folder.name
                        background: Rectangle { radius: Theme.radiusSmall; color: drop.containsDrag ? Theme.accentSoft : root.activeFolder===parent.modelData.folder.id ? Theme.hover : parent.hovered ? Theme.hover : "transparent" }
                        contentItem: RowLayout {
                            spacing: Theme.sm
                            IconButton { compact: true; visible: modelData.folder.childCount>0; iconName: root.expanded[modelData.folder.id] ? "minus" : "plus"; label: "Mostrar subpastas"; onClicked: { const next=Object.assign({},root.expanded); next[modelData.folder.id]=!next[modelData.folder.id]; root.expanded=next } }
                            Icon { name: "folder"; color: modelData.folder.color }
                            Text { Layout.fillWidth: true; text: modelData.folder.name; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.body }
                        }
                        onClicked: { root.revealFolder(modelData.folder.id); root.folderRequested(modelData.folder.id) }
                        DropArea {
                            id: drop; anchors.fill: parent; keys: ["scalar-project"]
                            onDropped: event => { if(!event.source || !event.source.projectId)return; const id=event.source.projectId,folder=parent.modelData.folder.id; event.accept(); Qt.callLater(function(){App.moveProjectToFolder(id,folder)}) }
                        }
                    }
                }
                Text { visible: App.folders.length===0; text: "Organize seus quadros\ncriando uma pasta."; color: Theme.secondary; font.pixelSize: Theme.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        Text { text: "Salvo neste dispositivo"; color: Theme.secondary; font.pixelSize: Theme.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    Rectangle { anchors.right: parent.right; width: Theme.hairline; height: parent.height; color: Theme.border }
}
