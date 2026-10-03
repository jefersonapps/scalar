import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "trashDialog"
    signal permanentDeletionRequested(var entry)
    contentItem: ColumnLayout {
        spacing: Theme.md
        RowLayout {
            Text { text: "Lixeira"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold; Layout.fillWidth: true }
            C.IconButton { iconName: "close"; label: "Fechar lixeira"; onClicked: root.close() }
        }
        Text { text: "Quadros excluídos ficam aqui por 30 dias."; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        ListView {
            id: list
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(Theme.section*6,Math.max(Theme.section*2,contentHeight)); clip: true; spacing: Theme.sm
            model: App.trashedProjects
            delegate: C.GlassPanel {
                required property var modelData
                width: list.width; implicitHeight: row.implicitHeight+Theme.md*2; radius: Theme.radiusMedium
                RowLayout {
                    id: row; anchors.fill: parent; anchors.margins: Theme.md; spacing: Theme.sm
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: Theme.xs
                        Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: "Expira em " + Qt.formatDate(new Date(new Date(modelData.deletedAt).getTime()+30*24*60*60*1000),"dd MMM yyyy"); color: Theme.secondary; font.pixelSize: Theme.caption }
                    }
                    C.IconButton { objectName: "restoreTrashedProject"; iconName: "undo"; label: "Restaurar quadro"; enabled: !App.busy; onClicked: App.restoreProject(modelData.id) }
                    C.IconButton { objectName: "deleteTrashedProject"; iconName: "trash"; label: "Excluir permanentemente"; enabled: !App.busy; onClicked: root.permanentDeletionRequested(modelData) }
                }
            }
            ScrollBar.vertical: ScrollBar {}
            Text { anchors.centerIn: parent; visible: list.count === 0; text: "Sua lixeira está vazia."; color: Theme.secondary; font.pixelSize: Theme.body }
        }
        Text { text: App.status; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
}
