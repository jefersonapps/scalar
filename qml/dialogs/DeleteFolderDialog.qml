import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "deleteFolderDialog"
    property var entry: ({})
    signal deletionConfirmed(string id, string parentId)
    function ask(folder) { entry=folder; open() }
    contentItem: ColumnLayout {
        spacing: Theme.lg
        Text { text: "Mover pasta para a lixeira?"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { text: root.entry.name || ""; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { text: "A pasta, seus quadros e suas subpastas ficarão na lixeira por 30 dias. Você poderá restaurar tudo junto durante esse período."; color: Theme.secondary; font.pixelSize: Theme.body; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        RowLayout {
            Layout.fillWidth: true
            C.ActionButton { text: "Cancelar"; Layout.fillWidth: true; onClicked: root.close() }
            C.ActionButton { objectName: "confirmFolderDeletion"; text: "Mover para lixeira"; primary: true; Layout.fillWidth: true; enabled: !App.busy; onClicked: { root.deletionConfirmed(root.entry.id,root.entry.parentId || ""); root.close() } }
        }
    }
}
