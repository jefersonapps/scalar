import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
import "../dialogs"
Item {
    id: root
    signal createRequested()
    signal openRequested()
    signal settingsRequested()
    DeleteProjectDialog { id: deletion }
    TrashDialog { id: trash; onPermanentDeletionRequested: entry => deletion.ask(entry,true) }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: width < Theme.mediumBreakpoint ? Theme.xl : Theme.section
        spacing: Theme.xxl
        Item {
            Layout.fillWidth: true
            implicitHeight: Theme.touch
            RowLayout {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.md
                Rectangle { Layout.preferredWidth: Theme.touch; Layout.preferredHeight: Theme.touch; color: Theme.accentSoft; radius: Theme.radiusMedium; C.Icon { anchors.centerIn: parent; name: "pen"; color: Theme.accent } }
                Text { text: "scalar"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold; font.letterSpacing: 1 }
            }
            RowLayout {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.lg
                Text { text: "ESPAÇO PARA IDEIAS"; visible: root.width > Theme.mediumBreakpoint; color: Theme.secondary; font.pixelSize: Theme.caption; font.letterSpacing: 2 }
                C.IconButton { objectName: "openTrashButton"; iconName: "trash"; label: "Lixeira (" + App.trashedProjects.length + ")"; onClicked: trash.open() }
                C.IconButton { iconName: "settings"; label: "Configurações"; onClicked: root.settingsRequested() }
            }
        }
        ColumnLayout {
            spacing: Theme.md
            Text { Layout.fillWidth: true; text: "Grandes ideias.\nUm quadro em branco."; color: Theme.text; font.pixelSize: root.width < Theme.compactBreakpoint ? Theme.heading : Theme.title; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
            Text { Layout.fillWidth: true; text: "Escreva, ensine e explore no seu ritmo."; color: Theme.secondary; font.pixelSize: Theme.body; wrapMode: Text.WordWrap }
            Flow {
                Layout.fillWidth: true; Layout.preferredHeight: childrenRect.height
                spacing: Theme.md
                C.ActionButton { objectName: "newProjectButton"; text: "Novo quadro"; primary: true; onClicked: root.createRequested() }
                C.ActionButton { text: "Criar com padrão"; enabled: !App.busy; onClicked: App.newDefault() }
                C.ActionButton { text: "Abrir projeto"; onClicked: root.openRequested() }
            }
        }
        RowLayout {
            Text { text: "Continue de onde parou"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.Medium }
            Item { Layout.fillWidth: true; implicitHeight: 1 }
            Text { text: "Neste dispositivo"; visible: root.width > Theme.compactBreakpoint; color: Theme.secondary; font.pixelSize: Theme.caption }
        }
        GridView {
            id: grid
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            cellWidth: Math.max(Theme.cardWidth, width/Math.max(1,Math.floor(width/Theme.cardWidth)))
            cellHeight: Theme.cardHeight+Theme.lg
            model: App.recentProjects
            delegate: C.ProjectCard {
                required property var modelData
                width: grid.cellWidth-Theme.lg
                projectName: modelData.name
                updated: modelData.updated
                thumbnail: modelData.thumbnail
                onClicked: App.openPath(modelData.path)
                onTrashRequested: deletion.ask(modelData,false)
            }
            ScrollBar.vertical: ScrollBar {}
            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.md
                visible: grid.count === 0
                C.Icon { Layout.alignment: Qt.AlignHCenter; name: "page"; color: Theme.secondary; width: Theme.xxl; height: Theme.xxl }
                Text { text: "Seu primeiro quadro está esperando."; color: Theme.secondary; font.pixelSize: Theme.body }
            }
        }
        Text { Layout.fillWidth: true; text: App.status; color: Theme.secondary; font.pixelSize: Theme.caption; elide: Text.ElideRight }
    }
}
