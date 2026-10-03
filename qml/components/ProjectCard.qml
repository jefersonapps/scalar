import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
AbstractButton {
    id: root
    property string projectName: ""
    property string updated: ""
    property url thumbnail: ""
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
        Text { Layout.fillWidth: true; text: root.projectName; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
        Text { text: "Alterado em " + Qt.formatDateTime(new Date(root.updated), "dd MMM · hh:mm"); color: Theme.secondary; font.pixelSize: Theme.caption }
    }
    padding: Theme.lg
}
