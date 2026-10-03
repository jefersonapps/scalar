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
    signal imageRequested()
    function insertImage(url) { board.importImage(url) }
    signal settingsRequested()
    Rectangle { anchors.fill: parent; color: Theme.workspace }
    BoardCanvas {
        id: board
        objectName: "boardCanvas"
        anchors.fill: parent
        anchors.topMargin: Theme.topbarHeight
        controller: App
        enabled: !root.overlaysOpen && !App.loading && !penOptions.visible && !shapeOptions.visible && !eraserOptions.visible
        toolbarExclusion: Qt.rect(toolbar.x, toolbar.y-y, toolbar.width, toolbar.height)
        optionsExclusion: properties.visible ? Qt.rect(properties.x,properties.y-y,properties.width,properties.height) : Qt.rect(0,0,0,0)
        Component.onCompleted: Qt.callLater(fitPage)
    }
    Item {
        anchors.fill: board
        Rectangle { x: board.selectionRect.x; y: board.selectionRect.y; width: board.selectionRect.width; height: board.selectionRect.height; visible: width > 0 || height > 0; color: "transparent"; border.color: Theme.accent; border.width: Theme.hairline }
        Repeater {
            model: board.selectionHandles
            Rectangle {
                required property var modelData
                x: modelData.x-Theme.handleSize/2; y: modelData.y-Theme.handleSize/2
                width: Theme.handleSize; height: Theme.handleSize
                radius: modelData.type === "rotate" || modelData.type === "center" ? Theme.handleSize/2 : Theme.xs/2
                color: Theme.surface; border.color: Theme.accent; border.width: Theme.focusBorder
            }
        }
    }
    Rectangle {
        x: board.x+board.eraserPosition.x-width/2; y: board.y+board.eraserPosition.y-height/2
        width: board.eraserRadius*2*board.zoom*96/25.4; height: width; radius: width/2
        visible: board.tool === "eraser"
        color: "#20ffffff"; border.color: Theme.accent; border.width: Theme.hairline
    }
    DropArea { anchors.fill: board; onDropped: drop => { if(drop.hasUrls && drop.urls.length > 0) { board.importImage(drop.urls[0]); drop.acceptProposedAction() } } }
    PropertiesPanel { id: properties; canvas: board; visible: board.selectedCount > 0 && board.tool === "select"; anchors.top: parent.top; anchors.right: parent.right; anchors.topMargin: Theme.topbarHeight+Theme.lg; anchors.rightMargin: Theme.lg }
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
        onShapeOptionsRequested: shapeOptions.open()
        onEraserOptionsRequested: eraserOptions.open()
        onImageRequested: root.imageRequested()
    }
    PenOptions { id: penOptions; canvas: board; x: Math.max(Theme.lg, (root.width-width)/2); y: toolbar.y-height-Theme.md }
    ShapeOptions { id: shapeOptions; canvas: board; x: Math.max(Theme.lg, (root.width-width)/2); y: toolbar.y-height-Theme.md }
    C.GlassPopover {
        id: eraserOptions; x: Math.max(Theme.lg,(root.width-width)/2); y: toolbar.y-height-Theme.md
        contentItem: ColumnLayout {
            Text { text: "Borracha por trecho"; color: Theme.text; font.pixelSize: Theme.body }
            Text { text: "Raio · " + board.eraserRadius.toFixed(1) + " mm"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.ModernSlider { from: 0.5; to: 12; value: board.eraserRadius; onMoved: board.eraserRadius = value; Layout.preferredWidth: Theme.propertiesWidth }
        }
    }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: Theme.lg
        width: Math.min(implicitWidth, root.width-Theme.xxl)
        text: board.drawing ? board.interactionHint : App.status
        color: Theme.secondary; font.pixelSize: Theme.caption; elide: Text.ElideRight
    }
    Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: Theme.lg; visible: root.width > Theme.hintBreakpoint; text: "Espaço + arraste para mover · rolagem para zoom"; color: Theme.secondary; font.pixelSize: Theme.caption }
    Shortcut { sequence: "E"; enabled: !root.overlaysOpen; onActivated: board.tool = "eraser" }
    Shortcut { sequence: "Ctrl+V"; enabled: !root.overlaysOpen; onActivated: board.pasteImage() }
    Shortcut { sequence: "V"; enabled: !root.overlaysOpen; onActivated: board.tool = "select" }
    Shortcut { sequence: "Delete"; enabled: !root.overlaysOpen && board.selectedCount > 0; onActivated: board.deleteSelection() }
    Shortcut { sequence: "Ctrl+D"; enabled: !root.overlaysOpen && board.selectedCount > 0; onActivated: board.duplicateSelection() }
    Shortcut { sequence: "P"; enabled: !root.overlaysOpen; onActivated: board.tool = "pen" }
    Shortcut { sequence: "H"; enabled: !root.overlaysOpen; onActivated: board.tool = "hand" }
    Shortcut { sequence: "Ctrl+Z"; enabled: !root.overlaysOpen && !board.drawing; onActivated: App.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; enabled: !root.overlaysOpen && !board.drawing; onActivated: App.redo() }
    Shortcut { sequence: "Ctrl+S"; enabled: !root.overlaysOpen; onActivated: App.save() }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !root.overlaysOpen; onActivated: root.saveAsRequested() }
}
