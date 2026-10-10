import QtQuick
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPanel {
    id: root
    objectName: "floatingToolbar"
    property Item backdropSource: null
    readonly property bool plainAppearance: !App.toolbarBlurSupported || App.reducedEffects || App.disableToolbarBlur
    color: plainAppearance ? Theme.surface : "transparent"
    border.width: plainAppearance ? 1 : 0
    Loader {
        id: glass
        anchors.fill: parent
        active: root.backdropSource !== null && !root.plainAppearance
                && root.GraphicsInfo.api !== GraphicsInfo.Software
                && root.GraphicsInfo.api !== GraphicsInfo.Unknown
        source: active ? "ToolbarBlur.qml" : ""
        onLoaded: item.toolbar = root
    }
    Rectangle {
        anchors.fill: parent
        visible: !root.plainAppearance
        radius: root.radius
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.dark ? "#b3333338" : "#ccefffff" }
            GradientStop { position: 1; color: Theme.dark ? "#a61b1b20" : "#b3ffffff" }
        }
        border.width: 1
        border.color: Theme.dark ? "#38808088" : "#30808088"
    }
    property alias penButton: penButton
    property alias markerButton: markerButton
    property alias eraserButton: eraserButton
    property alias shapesButton: shapesButton
    property alias rulerButton: rulerButton
    property alias compassButton: compassButton
    property string activeTool: "pen"
    signal toolSelected(string tool)
    signal penOptionsRequested()
    signal shapeOptionsRequested()
    signal eraserOptionsRequested()
    signal imageRequested()
    signal geometryOptionsRequested()
    implicitWidth: row.implicitWidth+Theme.xl
    width: Math.min(implicitWidth,parent.width-Theme.xxl)
    implicitHeight: Theme.toolbarHeight
    Flickable {
        id: scroll
        anchors.fill: parent; anchors.leftMargin: Theme.sm; anchors.rightMargin: Theme.sm; anchors.topMargin: Theme.xs; anchors.bottomMargin: Theme.xs
        contentWidth: row.implicitWidth; contentHeight: height
        interactive: contentWidth > width; clip: true
        boundsBehavior: Flickable.StopAtBounds
    RowLayout {
        id: row
        y: (scroll.height-height)/2
        spacing: Theme.xs
        C.ToolButton { iconName: "select"; label: "Selecionar (V) · Shift para seleção múltipla"; selected: root.activeTool === "select"; onClicked: root.toolSelected("select") }
        C.ToolButton { id: penButton;objectName: "penToolButton"; iconName: "pen"; label: "Caneta (P) · clique novamente para opções"; selected: root.activeTool === "pen"; onClicked: { if(root.activeTool === "pen") root.penOptionsRequested(); else root.toolSelected("pen") } }
        C.ToolButton { id: markerButton;objectName: "markerToolButton"; iconName: "marker"; label: "Marcador (M) · Ctrl + clique para preencher uma região"; selected: root.activeTool === "marker"; onClicked: { if(selected) root.penOptionsRequested(); else root.toolSelected("marker") } }
        C.ToolButton { id: eraserButton;objectName: "eraserToolButton"; iconName: "eraser"; label: "Borracha por trecho (E)"; selected: root.activeTool === "eraser"; onClicked: { if(root.activeTool === "eraser") root.eraserOptionsRequested(); else root.toolSelected("eraser") } }
        C.ToolButton { iconName: "hand"; label: "Mover quadro (H)"; selected: root.activeTool === "hand"; onClicked: root.toolSelected("hand") }
        C.ToolButton { iconName: "text"; label: "Texto e LaTeX (T)"; selected: root.activeTool === "text"; onClicked: root.toolSelected("text") }
        C.IconButton { iconName: "image"; label: "Importar imagem"; onClicked: root.imageRequested() }
        C.ToolButton { id: shapesButton;objectName: "shapesToolButton"; iconName: "shapes"; label: "Formas geométricas"; selected: ["line","circle","ellipse","triangle","rectangle"].indexOf(root.activeTool) >= 0; onClicked: root.shapeOptionsRequested() }
        C.ToolButton { id: rulerButton;objectName: "rulerToolButton"; iconName: "ruler"; label: "Régua · clique novamente para opções"; selected: root.activeTool === "ruler"; onClicked: { if(selected) root.geometryOptionsRequested(); else root.toolSelected("ruler") } }
        C.ToolButton { id: compassButton;objectName: "compassToolButton"; iconName: "compass"; label: "Compasso · clique novamente para opções"; selected: root.activeTool === "compass"; onClicked: { if(selected) root.geometryOptionsRequested(); else root.toolSelected("compass") } }
        Rectangle { width: 1; height: Theme.xl; color: Theme.border }
        C.IconButton { iconName: "undo"; label: "Desfazer (Ctrl+Z)"; enabled: App.canUndo; onClicked: App.undo() }
        C.IconButton { iconName: "redo"; label: "Refazer (Ctrl+Shift+Z)"; enabled: App.canRedo; onClicked: App.redo() }
    }
    }
}
