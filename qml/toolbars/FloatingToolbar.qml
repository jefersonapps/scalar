import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPanel {
    id: root
    property string activeTool: "pen"
    signal toolSelected(string tool)
    signal penOptionsRequested()
    signal shapeOptionsRequested()
    signal eraserOptionsRequested()
    signal imageRequested()
    implicitWidth: row.implicitWidth+Theme.xl
    implicitHeight: Theme.toolbarHeight
    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: Theme.sm
        C.ToolButton { iconName: "select"; label: "Selecionar (V) · Shift para seleção múltipla"; selected: root.activeTool === "select"; onClicked: root.toolSelected("select") }
        C.ToolButton { iconName: "pen"; label: "Caneta (P) · clique novamente para opções"; selected: root.activeTool === "pen"; onClicked: { if(root.activeTool === "pen") root.penOptionsRequested(); else root.toolSelected("pen") } }
        C.ToolButton { iconName: "eraser"; label: "Borracha por trecho (E)"; selected: root.activeTool === "eraser"; onClicked: { if(root.activeTool === "eraser") root.eraserOptionsRequested(); else root.toolSelected("eraser") } }
        C.IconButton { iconName: "image"; label: "Importar imagem"; onClicked: root.imageRequested() }
        C.ToolButton { iconName: "shapes"; label: "Formas geométricas"; selected: ["line","circle","ellipse","triangle","rectangle"].indexOf(root.activeTool) >= 0; onClicked: root.shapeOptionsRequested() }
        C.ToolButton { iconName: "hand"; label: "Mover quadro (H)"; selected: root.activeTool === "hand"; onClicked: root.toolSelected("hand") }
        Rectangle { width: 1; height: Theme.xl; color: Theme.border }
        C.IconButton { iconName: "undo"; label: "Desfazer (Ctrl+Z)"; enabled: App.canUndo; onClicked: App.undo() }
        C.IconButton { iconName: "redo"; label: "Refazer (Ctrl+Shift+Z)"; enabled: App.canRedo; onClicked: App.redo() }
    }
}
