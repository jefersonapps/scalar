import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Scalar 1.0
import "../components" as C
import "../toolbars"
import "../panels"
import "../theme"
Item {
    id: root
    property bool overlaysOpen: false
    signal saveAsRequested()
    signal settingsRequested()
    Rectangle { anchors.fill: parent; color: Theme.workspace }
    BoardCanvas {
        id: board
        objectName: "boardCanvas"
        anchors.fill: parent
        anchors.topMargin: Theme.topbarHeight
        controller: App
        enabled: !root.overlaysOpen && !App.loading && !penOptions.visible
        toolbarExclusion: Qt.rect(toolbar.x, toolbar.y-y, toolbar.width, toolbar.height)
        optionsExclusion: Qt.rect(penOptions.x,penOptions.y-y,penOptions.width,penOptions.height)
        Component.onCompleted: Qt.callLater(fitPage)
    }
    RowLayout {
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        anchors.margins: Theme.lg
        spacing: Theme.sm
        C.IconButton { iconName: "back"; label: "Voltar aos projetos"; enabled: !App.busy; onClicked: { board.cancelStroke(); App.home() } }
        ColumnLayout {
            Layout.fillWidth: true; spacing: Theme.xs
            Text { text: App.projectName; Layout.fillWidth: true; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
            Text { text: App.pageWidth.toFixed(1) + " × " + App.pageHeight.toFixed(1) + " mm"; color: Theme.secondary; font.pixelSize: Theme.caption }
        }
        C.IconButton { iconName: "save"; label: "Salvar como (Ctrl+Shift+S)"; enabled: !App.busy; onClicked: { board.cancelStroke(); root.saveAsRequested() } }
        C.IconButton { iconName: "minus"; label: "Diminuir zoom"; visible: root.width > Theme.zoomBreakpoint; onClicked: board.zoomBy(1/1.2) }
        Text { text: Math.round(board.zoom*100) + "%"; color: Theme.secondary; font.pixelSize: Theme.caption; visible: root.width > Theme.minimumWidth }
        C.IconButton { iconName: "plus"; label: "Aumentar zoom"; visible: root.width > Theme.zoomBreakpoint; onClicked: board.zoomBy(1.2) }
        C.IconButton { iconName: "fit"; label: "Ajustar página"; onClicked: board.fitPage() }
        C.IconButton { iconName: "settings"; label: "Configurações"; onClicked: { board.cancelStroke(); root.settingsRequested() } }
    }
    FloatingToolbar {
        id: toolbar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.xxl
        activeTool: board.tool
        onToolSelected: tool => { board.tool = tool; board.forceActiveFocus() }
        onPenOptionsRequested: penOptions.open()
    }
    PenOptions { id: penOptions; canvas: board; x: Math.max(Theme.lg, (root.width-width)/2); y: toolbar.y-height-Theme.md }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: Theme.lg
        width: Math.min(implicitWidth, root.width-Theme.xxl)
        text: board.drawing ? "Escrevendo…" : App.status
        color: Theme.secondary; font.pixelSize: Theme.caption; elide: Text.ElideRight
    }
    Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: Theme.lg; visible: root.width > Theme.hintBreakpoint; text: "Espaço + arraste para mover · rolagem para zoom"; color: Theme.secondary; font.pixelSize: Theme.caption }
    Shortcut { sequence: "P"; enabled: !root.overlaysOpen; onActivated: board.tool = "pen" }
    Shortcut { sequence: "H"; enabled: !root.overlaysOpen; onActivated: board.tool = "hand" }
    Shortcut { sequence: "Ctrl+Z"; enabled: !root.overlaysOpen && !board.drawing; onActivated: App.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; enabled: !root.overlaysOpen && !board.drawing; onActivated: App.redo() }
    Shortcut { sequence: "Ctrl+S"; enabled: !root.overlaysOpen; onActivated: App.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !root.overlaysOpen; onActivated: root.saveAsRequested() }
}
