import QtQuick
import QtQuick.Layouts
import "../theme"
GlassPanel {
    id: root
    signal previousRequested()
    signal nextRequested()
    signal overviewRequested()
    implicitWidth: controls.implicitWidth + Theme.sm * 2
    implicitHeight: Theme.touch + Theme.xs * 2
    RowLayout {
        id: controls
        anchors.centerIn: parent
        spacing: Theme.xs
        IconButton { objectName: "previousPageButton"; iconName: "back"; label: "Página anterior (Ctrl+PgUp)"; enabled: !App.busy && App.currentPage > 0; onClicked: root.previousRequested() }
        ActionButton { objectName: "pageCounterButton"; text: (App.currentPage+1) + " / " + App.pageCount; Accessible.name: "Página atual. Abrir todas as páginas"; onClicked: root.overviewRequested() }
        IconButton { objectName: "nextPageButton"; iconName: "arrow"; label: "Próxima página (Ctrl+PgDown)"; enabled: !App.busy && App.currentPage+1 < App.pageCount; onClicked: root.nextRequested() }
    }
}
