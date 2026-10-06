import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
AbstractButton {
    id: root
    objectName: "projectCard_" + projectId
    property string projectName: ""
    property string projectId: ""
    property string updated: ""
    property url thumbnail: ""
    signal editRequested()
    implicitWidth: Theme.cardWidth
    implicitHeight: Theme.cardHeight
    hoverEnabled: true
    Accessible.name: "Abrir " + projectName
    background: Rectangle {
        color: root.hovered ? Theme.hover : Theme.surface
        radius: Theme.radiusLarge
        border.color: root.hovered ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.fast } }
    }
    contentItem: ColumnLayout {
        anchors.margins: Theme.md
        spacing: Theme.md
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            color: Theme.background; radius: Theme.radiusMedium; clip: true
            Image { anchors.fill: parent; anchors.margins: Theme.sm; source: root.thumbnail; fillMode: Image.PreserveAspectFit; asynchronous: true; cache: false }
            Icon { anchors.centerIn: parent; name: "page"; color: Theme.secondary; visible: root.thumbnail.toString().length === 0 }
        }
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true; spacing: Theme.xs
                Text { Layout.fillWidth: true; text: root.projectName; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
                Text { text: Qt.formatDateTime(new Date(root.updated), "dd MMM · hh:mm"); color: Theme.secondary; font.pixelSize: Theme.caption }
            }
            IconButton { objectName: "editProjectButton_"+root.projectId; iconName: "settings"; label: "Editar " + root.projectName; enabled: !App.busy; onClicked: root.editRequested() }
        }
    }
    padding: Theme.lg
    DragHandler {
        id: dragHandler; target: null
        onActiveChanged: { if(active)dragPreview.Drag.active=true; else dragPreview.Drag.drop() }
    }
    Rectangle {
        id: dragPreview
        parent: Overlay.overlay
        visible: dragHandler.active
        width: Theme.folderCardWidth; height: Theme.touch; radius: Theme.radiusMedium
        color: Theme.surface; border.color: Theme.accent; border.width: Theme.focusBorder
        x: root.mapToItem(parent,dragHandler.centroid.position).x-width/2
        y: root.mapToItem(parent,dragHandler.centroid.position).y-height/2
        Drag.source: root; Drag.keys: ["scalar-project"]; Drag.hotSpot.x: width/2; Drag.hotSpot.y: height/2
        Text { anchors.fill: parent; anchors.margins: Theme.md; text: root.projectName; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.body; verticalAlignment: Text.AlignVCenter }
    }
}
