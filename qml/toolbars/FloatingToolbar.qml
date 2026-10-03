import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPanel {
    id: root
    property string activeTool: "pen"
    signal toolSelected(string tool)
    signal penOptionsRequested()
    implicitWidth: row.implicitWidth+Theme.xl
    implicitHeight: Theme.toolbarHeight
    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: Theme.sm
        C.ToolButton { iconName: "pen"; label: "Caneta (P) · clique novamente para opções"; selected: root.activeTool === "pen"; onClicked: { if(root.activeTool === "pen") root.penOptionsRequested(); else root.toolSelected("pen") } }
        C.ToolButton { iconName: "hand"; label: "Mover quadro (H)"; selected: root.activeTool === "hand"; onClicked: root.toolSelected("hand") }
        Rectangle { width: 1; height: Theme.xl; color: Theme.border }
        C.IconButton { iconName: "undo"; label: "Desfazer (Ctrl+Z)"; enabled: App.canUndo; onClicked: App.undo() }
        C.IconButton { iconName: "redo"; label: "Refazer (Ctrl+Shift+Z)"; enabled: App.canRedo; onClicked: App.redo() }
    }
}
