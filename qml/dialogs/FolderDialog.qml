import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "folderDialog"
    property string folderId: ""
    property string parentFolderId: ""
    property color folderColor: Theme.folderColors[0]
    property string error: ""
    signal deleteRequested(string folderId)
    function create(parentId) {
        folderId=""; parentFolderId=parentId || ""; nameField.text=""; folderColor=Theme.folderColors[0]
        error=""; open(); nameField.forceActiveFocus()
    }
    function edit(folder) {
        folderId=folder ? folder.id : ""
        parentFolderId=folder ? folder.parentId : ""
        nameField.text=folder ? folder.name : ""
        folderColor=folder ? folder.color : Theme.folderColors[0]
        error=""; open(); nameField.forceActiveFocus()
    }
    contentItem: ColumnLayout {
        spacing: Theme.lg
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: root.folderId ? "Editar pasta" : "Nova pasta"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
            C.IconButton { objectName: "deleteFolderButton"; visible: root.folderId!==""; iconName: "trash"; label: "Excluir pasta"; onClicked: root.deleteRequested(root.folderId) }
        }
        C.Field { id: nameField; objectName: "folderNameField"; Layout.fillWidth: true; placeholderText: "Nome da pasta"; maximumLength: 80; onAccepted: save.clicked() }
        Text { text: "Cor da pasta"; color: Theme.secondary; font.pixelSize: Theme.body }
        C.ColorPicker { Layout.fillWidth: true; colors: Theme.folderColors; selectedColor: root.folderColor; onPicked: value => root.folderColor=value }
        Text { Layout.fillWidth: true; visible: root.error.length>0; text: root.error; wrapMode: Text.WordWrap; color: Theme.danger; font.pixelSize: Theme.caption }
        RowLayout {
            Layout.fillWidth: true
            C.ActionButton { Layout.fillWidth: true; text: "Cancelar"; onClicked: root.close() }
            C.ActionButton {
                id: save; objectName: "saveFolderButton"; Layout.fillWidth: true; text: root.folderId ? "Salvar" : "Criar pasta"; primary: true; enabled: nameField.text.trim().length>0
                onClicked: { if(!enabled)return; if(App.saveFolder(root.folderId,nameField.text,root.folderColor.toString(),root.parentFolderId))root.close(); else root.error="Use um nome diferente para a pasta." }
            }
        }
    }
}
