import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "editProjectDialog"
    property var entry: ({})
    property string error: ""
    signal trashRequested(var project)
    function edit(project) { entry=project; nameField.text=project.name; error=""; open(); nameField.forceActiveFocus(); nameField.selectAll() }
    contentItem: ColumnLayout {
        spacing: Theme.lg
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: "Editar quadro"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
            C.IconButton { objectName: "trashProjectButton"; iconName: "trash"; label: "Mover quadro para a lixeira"; enabled: !App.busy; onClicked: { root.close(); root.trashRequested(root.entry) } }
        }
        Text { text: "Nome do quadro"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.Field { id: nameField; objectName: "editProjectNameField"; Layout.fillWidth: true; maximumLength: 120; onAccepted: save.clicked() }
        Text { Layout.fillWidth: true; text: root.error; visible: root.error!==""; color: Theme.danger; wrapMode: Text.WordWrap; font.pixelSize: Theme.caption }
        RowLayout {
            Layout.fillWidth: true
            C.ActionButton { Layout.fillWidth: true; text: "Cancelar"; onClicked: root.close() }
            C.ActionButton { id: save; objectName: "saveProjectNameButton"; Layout.fillWidth: true; primary: true; text: "Salvar"; enabled: !App.busy&&nameField.text.trim().length>0; onClicked: { if(enabled&&App.renameProject(root.entry.id,nameField.text))root.close(); else root.error="Não foi possível salvar o nome." } }
        }
    }
}
