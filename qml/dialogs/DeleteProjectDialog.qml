import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "deleteProjectDialog"
    property var entry: ({})
    property bool permanent: false
    function ask(project, forever) { entry = project; permanent = forever; open() }
    contentItem: ColumnLayout {
        spacing: Theme.lg
        Text { text: root.permanent ? "Excluir permanentemente?" : "Mover para a lixeira?"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { text: root.entry.name || ""; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { text: root.permanent ? "O quadro e suas imagens serão removidos deste dispositivo. Esta ação não pode ser desfeita." : "Você poderá restaurar este quadro durante 30 dias. Após esse prazo, ele será excluído automaticamente."; color: Theme.secondary; font.pixelSize: Theme.body; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { text: root.entry.originalPath || root.entry.path || ""; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
        RowLayout {
            C.ActionButton { objectName: "cancelProjectDeletion"; text: "Cancelar"; Layout.fillWidth: true; onClicked: root.close() }
            C.ActionButton { objectName: "confirmProjectDeletion"; text: root.permanent ? "Excluir definitivamente" : "Mover para lixeira"; primary: true; Layout.fillWidth: true; enabled: !App.busy; onClicked: { if(root.permanent) App.deleteProjectPermanently(root.entry.id); else App.trashProject(root.entry.id); root.close() } }
        }
    }
}
