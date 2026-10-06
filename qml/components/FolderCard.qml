import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
AbstractButton {
    id: root
    objectName: "folderCard_" + folderId
    property string folderId: ""
    property string folderName: ""
    property color folderColor: Theme.accent
    property int projectCount: 0
    signal editRequested()
    implicitWidth: Theme.folderCardWidth; implicitHeight: Theme.folderCardHeight
    hoverEnabled: true; padding: Theme.md
    Accessible.name: "Abrir pasta " + folderName
    background: Rectangle {
        radius: Theme.radiusMedium
        color: Qt.rgba(root.folderColor.r,root.folderColor.g,root.folderColor.b,drop.containsDrag ? .24 : root.hovered ? .16 : .08)
        border.color: drop.containsDrag ? root.folderColor : Theme.border
        border.width: drop.containsDrag ? Theme.focusBorder : Theme.hairline
    }
    contentItem: RowLayout {
        spacing: Theme.sm
        Icon { name: "folder"; color: root.folderColor }
        ColumnLayout {
            Layout.fillWidth: true; spacing: Theme.xs
            Text { Layout.fillWidth: true; text: root.folderName; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
            Text { text: root.projectCount + (root.projectCount===1 ? " quadro" : " quadros"); color: Theme.secondary; font.pixelSize: Theme.caption }
        }
        IconButton { objectName: "editFolder_"+root.folderId; visible: root.folderId!==""; compact: true; iconName: "settings"; label: "Nome e cor da pasta"; onClicked: root.editRequested() }
    }
    DropArea {
        id: drop; anchors.fill: parent; keys: ["scalar-project"]
        onDropped: event => {
            if(!event.source || !event.source.projectId)return
            const project=event.source.projectId,folder=root.folderId
            event.accept()
            Qt.callLater(function(){ App.moveProjectToFolder(project,folder) })
        }
    }
}
