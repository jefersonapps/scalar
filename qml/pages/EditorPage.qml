import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Scalar 1.0
import "../components" as C
import "../toolbars"
import "../panels"
import "../theme"
import "../dialogs"
Item {
    id: root
    property bool overlaysOpen: false
    function exportSelection(svg) {
        selectionMenu.close()
        selectionExport.svg = svg
        selectionExport.open()
    }
    FileDialog {
        id: selectionExport
        objectName: "selectionExportFileDialog"
        property bool svg: false
        title: svg ? "Exportar seleção como SVG" : "Exportar seleção como PDF"
        fileMode: FileDialog.SaveFile
        defaultSuffix: svg ? "svg" : "pdf"
        nameFilters: svg ? ["SVG (*.svg)"] : ["PDF (*.pdf)"]
        onAccepted: board.exportSelection(selectedFile, svg)
    }
    C.GlassPopover {
        id: selectionMenu
        objectName: "selectionContextMenu"
        width: 240
        contentItem: ColumnLayout {
            spacing: Theme.sm
            Text { text: "Exportar seleção"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.ActionButton { objectName: "contextExportSelectionPdfButton"; text: "Exportar como PDF"; Layout.fillWidth: true; enabled: !App.exporting; onClicked: root.exportSelection(false) }
            C.ActionButton { objectName: "contextExportSelectionSvgButton"; text: "Exportar como SVG"; Layout.fillWidth: true; enabled: !App.exporting; onClicked: root.exportSelection(true) }
        }
    }
    property var pressedTogglePanel: null
    function rememberPanelTrigger(position) {
        pressedTogglePanel=null
        const triggers=[[fileOptions,fileButton],[pagesPanel,pagesButton],[pagesPanel,navigator.overviewButton],
                        [backgroundOptions,backgroundButton],[penOptions,toolbar.penButton],[penOptions,toolbar.markerButton],
                        [eraserOptions,toolbar.eraserButton],[shapeOptions,toolbar.shapesButton],
                        [geometryOptions,toolbar.rulerButton],[geometryOptions,toolbar.compassButton]]
        for(const pair of triggers){
            const button=pair[1],local=button.mapFromItem(null,position.x,position.y)
            if(pair[0].expanded && button.visible && button.enabled && button.contains(local)){pressedTogglePanel=pair[0];break}
        }
    }
    function togglePanel(panel) {
        if(panel.expanded || pressedTogglePanel===panel)panel.close()
        else panel.open()
        pressedTogglePanel=null
    }
    property var afterTextEdit: null
    function finishTextThen(action) {
        if(inlineText.active){afterTextEdit=action;inlineText.finish()}
        else action()
    }
    function prepareToClose() {
        if(!inlineText.active)return true
        finishTextThen(function(){root.Window.window.close()})
        return false
    }
    readonly property bool inlineNavigation: width >= Theme.navigationInlineBreakpoint
    readonly property real headerRowHeight: fullScreen ? Theme.touch+Theme.sm*2 : Theme.topbarHeight
    readonly property real headerHeight: inlineNavigation ? headerRowHeight : headerRowHeight + navigator.implicitHeight + Theme.lg
    signal saveAsRequested()
    signal imageRequested()
    signal importPdfRequested()
    signal exportPdfRequested()
    function insertImage(url) { board.importImage(url) }
    function insertImages(urls) { board.importImages(urls) }
    signal settingsRequested()
    signal backRequested()
    signal fullScreenRequested()
    readonly property bool fullScreen: root.Window.window && root.Window.window.visibility === Window.FullScreen
    function finishNameEdit() {
        if(!projectTitle.editing)return
        App.renameCurrentProject(projectTitleInput.text)
        projectTitle.editing=false
    }
    TapHandler {
        onTapped: eventPoint => {
            const point=projectTitle.mapFromItem(root,eventPoint.position)
            if(projectTitle.editing && (point.x<0 || point.y<0 || point.x>projectTitle.width || point.y>projectTitle.height))root.finishNameEdit()
            const zoomPoint=zoomControl.mapFromItem(root,eventPoint.position)
            if(zoomControl.editing && (zoomPoint.x<0 || zoomPoint.y<0 || zoomPoint.x>zoomControl.width || zoomPoint.y>zoomControl.height))zoomControl.finish()
        }
    }
    Rectangle { anchors.fill: parent; color: Theme.workspace }
    BoardCanvas {
        id: board
        objectName: "boardCanvas"
        anchors.fill: parent
        anchors.topMargin: root.headerHeight
        onWindowPointerPressed: position => root.rememberPanelTrigger(position)
        onWindowPointerReleased: Qt.callLater(function(){root.pressedTogglePanel=null})
        controller: App
        wheelZoomEnabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !App.loading
        enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !App.loading && !fileOptions.visible && !pagesPanel.visible && !geometryOptions.visible && !penOptions.visible && !penOptions.colorDialogOpen && !shapeOptions.visible && !eraserOptions.visible && !backgroundOptions.visible && !backgroundOptions.colorDialogOpen
        toolbarExclusion: Qt.rect(toolbar.x, toolbar.y-y, toolbar.width, toolbar.height)
        optionsExclusion: properties.visible ? Qt.rect(properties.x,properties.y-y,properties.width,properties.height) : Qt.rect(0,0,0,0)
        onTextRequested: (position, id) => inlineText.begin(Qt.rect(position.x,position.y,60,18),id)
        onTextBoxRequested: box => inlineText.begin(box,"")
        onSelectionContextRequested: position => {
            const point = board.mapToItem(root, position.x, position.y)
            selectionMenu.x = Math.max(Theme.sm, Math.min(point.x, root.width - selectionMenu.width - Theme.sm))
            selectionMenu.y = Math.max(Theme.sm, Math.min(point.y, root.height - selectionMenu.implicitHeight - Theme.sm))
            selectionMenu.open()
        }
        Component.onCompleted: Qt.callLater(restorePageView)
    }
    Item {
        anchors.fill: board
        clip: true
        C.RulerGuide { canvas: board }
        C.CompassGuide { canvas: board }
        C.SegmentAngleGuide { canvas: board; anchors.fill: parent }
        Repeater {
            id: sectorLabels
            model: board.sectorAngles
            C.AngleLabel {
                required property var modelData
                objectName: "sectorAngleLabel"
                angle: modelData.angle
                x: modelData.x - width / 2; y: modelData.y - height / 2
            }
        }
        Connections { target: board; function onViewChanged() { sectorLabels.model = Qt.binding(() => board.sectorAngles) } }
        Rectangle {
            x: board.selectionRect.x; y: board.selectionRect.y
            width: board.selectionRect.width; height: board.selectionRect.height
            visible: width > 0 || height > 0
            radius: 3
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.035)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.8)
            border.width: 1.5
        }
        Rectangle {
            visible: board.selectedCount > 0
            x: board.selectionRect.x + board.selectionRect.width / 2
            y: board.selectionRect.y - 23
            width: 1; height: 23; color: Theme.accent
        }
        Repeater {
            model: board.selectionHandles
            Rectangle {
                required property var modelData
                x: modelData.x-Theme.handleSize/2; y: modelData.y-Theme.handleSize/2
                width: Theme.handleSize; height: Theme.handleSize
                radius: modelData.type === "rotate" || modelData.type === "center" ? Theme.handleSize/2 : 3
                color: Theme.surface; border.color: Theme.accent; border.width: Theme.focusBorder
            }
        }
    }
    Rectangle {
        objectName: "eraserIndicator"
        x: board.x+board.eraserPosition.x-width/2; y: board.y+board.eraserPosition.y-height/2
        width: board.eraserRadius*2*board.zoom*96/25.4; height: width; radius: width/2
        visible: board.eraserVisible
        color: "#20ffffff"; border.color: Theme.accent; border.width: Theme.hairline
    }
    DropArea {
        anchors.fill: board; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active
        onDropped: drop => {
            if(drop.hasUrls && drop.urls.length > 0) {
                if(drop.urls[0].toString().toLowerCase().endsWith(".pdf")) App.inspectPdfFile(drop.urls[0])
                else board.importImages(drop.urls)
                drop.acceptProposedAction()
            }
        }
    }
    PropertiesPanel { id: properties; canvas: board; onExportRequested: svg => root.exportSelection(svg); visible: board.selectedCount > 0 && board.tool === "select" && !inlineText.active; anchors.top: parent.top; anchors.right: parent.right; anchors.topMargin: root.headerHeight+Theme.lg; anchors.rightMargin: Theme.lg }
    RowLayout {
        id: topbar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: Theme.lg;anchors.rightMargin: Theme.lg
        height: root.headerRowHeight
        spacing: Theme.sm
        C.IconButton { objectName: "editorBackButton"; iconName: "back"; label: "Voltar aos projetos"; enabled: !App.documentBusy; onClicked: root.finishTextThen(function(){ board.cancelStroke();root.backRequested() }) }
        ColumnLayout {
            Layout.fillWidth: true; spacing: Theme.xs
            Item {
                id: projectTitle
                Layout.fillWidth: true; implicitHeight: Theme.controlHeight-Theme.md
                property bool editing: false
                Text { anchors.fill: parent; verticalAlignment: Text.AlignVCenter; visible: !projectTitle.editing; text: App.projectName; elide: Text.ElideRight; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
                MouseArea { objectName: "projectTitleArea"; anchors.fill: parent; visible: !projectTitle.editing; cursorShape: Qt.IBeamCursor; onClicked: { projectTitleInput.text=App.projectName; projectTitle.editing=true; projectTitleInput.forceActiveFocus(); projectTitleInput.selectAll() } }
                C.Field {
                    id: projectTitleInput; objectName: "projectTitleInput"
                    anchors.fill: parent; visible: projectTitle.editing; maximumLength: 120
                    onAccepted: { root.finishNameEdit(); board.forceActiveFocus() }
                    onActiveFocusChanged: if(!activeFocus && projectTitle.editing)root.finishNameEdit()
                    Keys.onEscapePressed: { projectTitle.editing=false; board.forceActiveFocus() }
                }
            }
            Text { text: App.pageInfinite ? "Quadro infinito" : App.pageWidth.toFixed(1) + " × " + App.pageHeight.toFixed(1) + " mm"; color: Theme.secondary; font.pixelSize: Theme.caption }
        }
        Item { id: navigationSpace; visible: root.inlineNavigation; Layout.preferredWidth: navigator.implicitWidth; Layout.preferredHeight: navigator.implicitHeight }
        C.ActionButton { id: fileButton;objectName: "fileMenuButton";text: "Arquivo";iconName: "folder";enabled: !App.documentBusy;onClicked: root.finishTextThen(function(){board.cancelStroke();root.togglePanel(fileOptions)}) }
        C.ActionButton { id: pagesButton;objectName: "openPagesButton"; visible: root.width > Theme.zoomBreakpoint; iconName: "pages"; text: "Páginas"; onClicked: root.finishTextThen(function(){board.cancelStroke();root.togglePanel(pagesPanel)}) }
        C.IconButton { iconName: "minus"; label: "Diminuir zoom"; visible: root.width > Theme.zoomBreakpoint; onClicked: board.zoomBy(1/1.2) }
        Item {
            id: zoomControl;objectName: "zoomControl"
            visible: root.width>Theme.minimumWidth
            Layout.preferredWidth: Theme.section+Theme.lg;Layout.preferredHeight: Theme.controlHeight
            property bool editing: false
            function finish() {
                if(!editing)return
                if(zoomInput.acceptableInput)board.setZoom(Number(zoomInput.text)/100)
                editing=false
            }
            Text { anchors.fill: parent;visible: !zoomControl.editing;text: Number((board.zoom*100).toFixed(1))+"%";color: Theme.secondary;font.pixelSize: Theme.caption;horizontalAlignment: Text.AlignHCenter;verticalAlignment: Text.AlignVCenter }
            MouseArea { objectName: "zoomValueArea";anchors.fill: parent;visible: !zoomControl.editing;cursorShape: Qt.IBeamCursor;onClicked: { zoomInput.text=Number((board.zoom*100).toFixed(2)).toString();zoomControl.editing=true;zoomInput.forceActiveFocus();zoomInput.selectAll() } }
            C.Field {
                id: zoomInput;objectName: "zoomValueInput"
                anchors.fill: parent;visible: zoomControl.editing
                leftPadding: Theme.xs;rightPadding: Theme.xs
                horizontalAlignment: Text.AlignHCenter
                validator: DoubleValidator { bottom: 5;top: 800;decimals: 2;locale: "C";notation: DoubleValidator.StandardNotation }
                Accessible.name: "Zoom em porcentagem, de 5 a 800"
                onAccepted: { zoomControl.finish();board.forceActiveFocus() }
                onActiveFocusChanged: if(!activeFocus)zoomControl.finish()
                Keys.onEscapePressed: { zoomControl.editing=false;board.forceActiveFocus() }
            }
        }
        C.IconButton { iconName: "plus"; label: "Aumentar zoom"; visible: root.width > Theme.zoomBreakpoint; onClicked: board.zoomBy(1.2) }
        C.IconButton { iconName: "fit"; label: "Ajustar página"; onClicked: board.fitPage() }
        C.IconButton { objectName: "fullScreenButton";iconName: root.fullScreen ? "restoreScreen" : "fullScreen";label: root.fullScreen ? "Sair da tela cheia (F11)" : "Tela cheia (F11)";selected: root.fullScreen;onClicked: root.fullScreenRequested() }
        C.IconButton { id: backgroundButton;objectName: "backgroundOptionsButton";iconName: "background"; label: "Personalizar fundo e grade da página"; onClicked: root.finishTextThen(function(){board.cancelStroke();root.togglePanel(backgroundOptions)}) }
        C.IconButton { iconName: "settings"; label: "Configurações"; onClicked: root.finishTextThen(function(){board.cancelStroke();root.settingsRequested()}) }
    }
    C.GlassPopover {
        id: fileOptions;objectName: "fileOptions"
        width: Theme.propertiesWidth
        onAboutToShow: { const point=fileButton.mapToItem(root,0,fileButton.height+Theme.sm);x=Math.max(Theme.lg,Math.min(point.x,root.width-width-Theme.lg));y=point.y }
        onClosed: board.forceActiveFocus()
        contentItem: ColumnLayout {
            spacing: Theme.sm
            Text { text: "Arquivo";color: Theme.text;font.pixelSize: Theme.body;font.weight: Font.DemiBold }
            C.ActionButton { text: "Salvar como…";iconName: "save";Layout.fillWidth: true;enabled: !App.documentBusy;onClicked: { fileOptions.close();root.saveAsRequested() } }
            C.ActionButton { objectName: "importPdfButton";text: "Importar PDF…";iconName: "pdfImport";Layout.fillWidth: true;enabled: !App.documentBusy;onClicked: { fileOptions.close();root.importPdfRequested() } }
            C.ActionButton { objectName: "exportPdfButton";text: "Exportar PDF…";iconName: "pdfExport";Layout.fillWidth: true;enabled: !App.documentBusy && !App.exporting;onClicked: { fileOptions.close();root.exportPdfRequested() } }
        }
    }
    FloatingToolbar {
        id: toolbar
        backdropSource: board
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.xxl
        activeTool: board.tool
        onToolSelected: tool => root.finishTextThen(function(){ board.tool=tool;board.forceActiveFocus() })
        onPenOptionsRequested: root.togglePanel(penOptions)
        onShapeOptionsRequested: root.togglePanel(shapeOptions)
        onEraserOptionsRequested: root.togglePanel(eraserOptions)
        onImageRequested: root.imageRequested()
        onGeometryOptionsRequested: root.togglePanel(geometryOptions)
    }
    C.PageNavigator {
        id: navigator
        x: root.inlineNavigation ? topbar.x + navigationSpace.x : (root.width-width)/2
        y: root.inlineNavigation ? topbar.y+(topbar.height-height)/2 : root.headerRowHeight + Theme.sm
        onPreviousRequested: root.finishTextThen(function(){board.cancelStroke();App.selectPage(App.currentPage-1);board.forceActiveFocus()})
        onNextRequested: root.finishTextThen(function(){board.cancelStroke();App.selectPage(App.currentPage+1);board.forceActiveFocus()})
        onAddRequested: root.finishTextThen(function(){board.cancelStroke();App.addPage();board.forceActiveFocus()})
        onOverviewRequested: root.finishTextThen(function(){board.cancelStroke();root.togglePanel(pagesPanel)})
    }
    C.InlineTextEditor {
        id: inlineText; anchors.fill: board; canvas: board
        onFinished: { const action=root.afterTextEdit;root.afterTextEdit=null;if(action)action() }
    }
    Row {
        visible: !inlineText.active && board.selectionName==="Texto" && board.selectedCount===1
        x: board.x+board.selectionRect.x
        y: Math.max(board.y,board.y+board.selectionRect.y-Theme.touch-Theme.xs)
        spacing: Theme.xs
        C.IconButton { objectName: "selectedTextFormatButton";iconName: "settings";label: "Fonte, tamanho e cor";selected: true;onClicked: { inlineText.begin(Qt.rect(0,0,60,18),board.selectedTextId());inlineText.openFormatting() } }
    }
    GeometryOptions {
        id: geometryOptions; canvas: board
        maximumHeight: Math.max(Theme.section*3,toolbar.y-root.headerHeight-Theme.lg*3)
        function reposition() { x=Math.max(Theme.lg,(root.width-width)/2); y=Math.max(Theme.lg,toolbar.y-height-Theme.md) }
        onAboutToShow: reposition()
        onHeightChanged: Qt.callLater(reposition)
        Connections {
            target: root
            function onWidthChanged() { Qt.callLater(geometryOptions.reposition) }
            function onHeightChanged() { Qt.callLater(geometryOptions.reposition) }
        }
        onClosed: board.forceActiveFocus()
    }
    PagesPanel { id: pagesPanel; x: Math.max(Theme.lg,root.width-width-Theme.lg); y: root.headerHeight; onClosed: board.forceActiveFocus() }
    BackgroundOptions { id: backgroundOptions; x: Math.max(Theme.lg,root.width-width-Theme.lg); y: root.headerHeight;onPageSizeChanged: board.fitPage() }
    PenOptions {
        id: penOptions; canvas: board
        function reposition() { x = Math.max(Theme.lg,(root.width-width)/2); y = Math.max(Theme.lg,toolbar.y-height-Theme.md) }
        onAboutToShow: reposition()
        onHeightChanged: Qt.callLater(reposition)
        Connections {
            target: root
            function onWidthChanged() { penOptions.reposition() }
            function onHeightChanged() { Qt.callLater(penOptions.reposition) }
        }
    }
    ShapeOptions { id: shapeOptions; canvas: board; x: Math.max(Theme.lg, (root.width-width)/2); y: Math.max(Theme.lg,toolbar.y-height-Theme.md) }
    C.GlassPopover {
        id: eraserOptions; objectName: "eraserOptions"; x: Math.max(Theme.lg,(root.width-width)/2); y: toolbar.y-height-Theme.md
        contentItem: ColumnLayout {
            Text { text: "Borracha por trecho"; color: Theme.text; font.pixelSize: Theme.body }
            Text { text: "Raio · " + board.eraserRadius.toFixed(1) + " mm"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.ModernSlider { from: 0.5; to: 12; value: board.eraserRadius; onMoved: board.eraserRadius = value; Layout.preferredWidth: Theme.propertiesWidth }
            Text { text: "+ ou ] para aumentar · - ou [ para diminuir\nSegure a tecla para ajustar · raio de 0,5 a 12 mm"; color: Theme.secondary; font.pixelSize: Theme.caption }
            C.ModernCheckBox { objectName: "eraseShapesCheckBox"; text: "Apagar formas também"; checked: board.eraserShapes; onToggled: board.eraserShapes=checked }
            Text { text: "Segure Ctrl para apagar trechos de formas\ntemporariamente."; color: Theme.secondary; font.pixelSize: Theme.caption }
            Text { text: "Segure Shift para restaurar trechos apagados."; color: Theme.secondary; font.pixelSize: Theme.caption }
        }
    }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: Theme.lg
        width: Math.min(implicitWidth, root.width-Theme.xxl)
        text: board.drawing ? board.interactionHint : App.status
        color: Theme.secondary; font.pixelSize: Theme.caption; elide: Text.ElideRight
    }
    Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: Theme.lg; visible: root.width > Theme.hintBreakpoint; text: "Rolagem para mover · Ctrl + rolagem para zoom"; color: Theme.secondary; font.pixelSize: Theme.caption }
    Shortcut { sequence: "E"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "eraser" }
    readonly property bool eraserSizeShortcutsEnabled: board.tool === "eraser" && (board.enabled || eraserOptions.visible) && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !projectTitle.editing && !zoomControl.editing
    Shortcut { sequences: ["+", "]"]; enabled: root.eraserSizeShortcutsEnabled; autoRepeat: true; onActivated: board.adjustEraserSize(true) }
    Shortcut { sequences: ["-", "["]; enabled: root.eraserSizeShortcutsEnabled; autoRepeat: true; onActivated: board.adjustEraserSize(false) }
    Shortcut { sequence: "Ctrl+V"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.pasteImage() }
    Shortcut { sequence: "V"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "select" }
    Shortcut { sequence: "Delete"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && (board.selectedCount > 0 || board.selectedGuide !== ""); onActivated: board.deleteSelection() }
    Shortcut { sequence: "Ctrl+D"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && board.selectedCount > 0; onActivated: board.duplicateSelection() }
    Shortcut { sequence: "Ctrl+Up"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && board.selectedCount > 0; onActivated: board.moveSelectionLayer(true) }
    Shortcut { sequence: "Ctrl+Down"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && board.selectedCount > 0; onActivated: board.moveSelectionLayer(false) }
    Shortcut { sequence: "T"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "text" }
    Shortcut { sequence: "P"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "pen" }
    Shortcut { sequence: "M"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "marker" }
    Shortcut { sequence: "Ctrl+Z"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !board.drawing; onActivated: App.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !board.drawing; onActivated: App.redo() }
    Shortcut { sequence: "Ctrl+S"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible; onActivated: root.finishTextThen(function(){App.save()}) }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: root.saveAsRequested() }
    Shortcut { sequence: "Ctrl+PgDown"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: { board.cancelStroke(); App.selectPage(App.currentPage+1) } }
    Shortcut { sequence: "Ctrl+PgUp"; enabled: !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: { board.cancelStroke(); App.selectPage(App.currentPage-1) } }
}
