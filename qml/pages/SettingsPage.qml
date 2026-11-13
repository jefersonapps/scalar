import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
Item {
    id: root
    objectName: "settingsPage"
    property bool showBackButton: false
    signal backRequested()
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.xl
        spacing: Theme.md
        C.ActionButton {
            visible: root.showBackButton
            text: "Voltar aos quadros"
            iconName: "back"
            onClicked: root.backRequested()
        }
        C.SettingsContent {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 900
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
