import QtQuick
import QtQuick.Layouts
import "../theme"
GlassPanel {
    id: root
    property alias overviewButton: counterButton
    signal previousRequested()
    signal nextRequested()
    signal addRequested()
    signal overviewRequested()
    implicitWidth: controls.implicitWidth + Theme.sm * 2
    implicitHeight: Theme.touch + Theme.xs * 2
    RowLayout {
        id: controls
        anchors.centerIn: parent
        spacing: Theme.xs
        IconButton { objectName: "previousPageButton"; iconName: "back"; label: "Página anterior (Ctrl+PgUp)"; enabled: !App.documentBusy && App.currentPage > 0; onClicked: root.previousRequested() }
        ActionButton { id: counterButton;objectName: "pageCounterButton"; text: (App.currentPage+1) + " / " + App.pageCount; Accessible.name: "Página atual. Abrir todas as páginas"; onClicked: root.overviewRequested() }
        IconButton { objectName: "newPageButton"; iconName: "pageAdd"; label: "Nova página"; enabled: !App.documentBusy && App.pageCount < 1000; onClicked: root.addRequested() }
        IconButton { objectName: "nextPageButton"; iconName: "arrow"; label: "Próxima página (Ctrl+PgDown)"; enabled: !App.documentBusy && App.currentPage+1 < App.pageCount; onClicked: root.nextRequested() }
    }
}
