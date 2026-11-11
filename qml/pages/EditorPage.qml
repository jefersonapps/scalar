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
    readonly property var recorder: App.constructionRecorder
    property bool overlaysOpen: false
    property bool browserOpen: false
    property bool browserCreated: false
    property real browserPreferredWidth: 460
    readonly property real browserWidth: browserOpen ? Math.min(Math.max(300,browserPreferredWidth),Math.max(300,width-320)) : 0
    readonly property bool browserFocused: browserOpen && browserLoader.item && browserLoader.item.browserFocused
    function closeBrowser() { browserOpen=false;board.forceActiveFocus() }
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
    function importDroppedProject(url) {
        finishNameEdit()
        if(inlineText.active){afterTextEdit=function(){App.requestImportProject(url)};inlineText.finishForImport()}
        else App.requestImportProject(url)
    }
    readonly property real headerRowHeight: Theme.toolbarHeight
    readonly property real headerHeight: headerRowHeight
    signal saveAsRequested()
    signal importBoardRequested()
    signal imageRequested()
    signal importPdfRequested()
    signal exportPdfRequested()
    function insertImage(url) { board.importImage(url) }
    function insertImages(urls) { board.importImages(urls) }
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
        anchors.rightMargin: root.browserOpen ? root.browserWidth+6 : 0
        onWindowPointerPressed: position => root.rememberPanelTrigger(position)
        onWindowPointerReleased: Qt.callLater(function(){root.pressedTogglePanel=null})
        controller: App
        wheelZoomEnabled: !toolOptions.textPopupOpen && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.formattingOpen && !App.loading
        enabled: !App.constructionPlayback && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !App.loading && !App.pageImagesLoading && !fileOptions.visible && !pagesPanel.visible && !geometryOptions.visible && !penOptions.visible && !penOptions.colorDialogOpen && !shapeOptions.visible && !eraserOptions.visible && !backgroundOptions.visible && !backgroundOptions.colorDialogOpen
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
        Repeater {
            model: board.magneticAnchors
            Rectangle {
                required property var modelData
                x: modelData.x-width/2; y: modelData.y-height/2
                width: 16; height: 16; radius: 8
                color: "transparent"; border.width: 2; border.color: Theme.accent
            }
        }
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
            objectName: "selectionFrame"
            x: board.selectionFrame.x; y: board.selectionFrame.y
            width: board.selectionFrame.width; height: board.selectionFrame.height
            rotation: board.selectionRotation; transformOrigin: Item.TopLeft
            visible: !inlineText.active && (width > 0 || height > 0)
            radius: 3
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.035)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.8)
            border.width: 1.5
            Rectangle {
                visible: board.selectedCount > 0
                x: parent.width / 2; y: -23
                width: 1; height: 23; color: Theme.accent
            }
        }
        Repeater {
            objectName: "selectionHandleRepeater"
            model: inlineText.active ? [] : board.selectionHandles
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
        id: eraserIndicator
        objectName: "eraserIndicator"
        property point lastVisiblePosition: Qt.point(0,0)
        function trackPosition() { if(board.eraserVisible)lastVisiblePosition=board.eraserPosition }
        x: board.x+lastVisiblePosition.x-width/2; y: board.y+lastVisiblePosition.y-height/2
        width: board.eraserRadius*2*board.zoom*96/25.4; height: width; radius: width/2
        visible: board.eraserVisible
        color: "#20ffffff"; border.color: Theme.accent; border.width: Theme.hairline
        Connections {
            target: board
            function onSelectionChanged() { eraserIndicator.trackPosition() }
            function onViewChanged() { eraserIndicator.trackPosition() }
        }
    }
    DropArea {
        objectName: "boardImageDropArea"
        anchors.fill: board; enabled: !App.constructionPlayback && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active
        onEntered: drag => {
            if(drag.hasUrls || drag.formats.indexOf("text/html")>=0) drag.acceptProposedAction()
            else drag.accepted = false
        }
        onDropped: drop => {
            if(drop.hasUrls || drop.formats.indexOf("text/html")>=0) {
                if(drop.hasUrls && drop.urls.length===1 && App.canImportProject(drop.urls[0]))root.importDroppedProject(drop.urls[0])
                else if(drop.hasUrls && drop.urls.length>0 && drop.urls[0].toString().toLowerCase().endsWith(".pdf")) App.inspectPdfFile(drop.urls[0])
                else board.importDroppedImages(drop.hasUrls ? drop.urls : [], drop.getDataAsString("text/html"),Qt.point(drop.x,drop.y))
                drop.accept(Qt.CopyAction)
            }
        }
    }
    Loader {
        id: browserLoader; objectName: "browserPanelLoader"
        anchors.top: parent.top; anchors.topMargin: root.headerHeight
        anchors.right: parent.right; anchors.bottom: parent.bottom
        width: root.browserWidth; visible: root.browserOpen; active: root.browserCreated
        sourceComponent: BrowserPanel {
            suspended: root.overlaysOpen || selectionExport.visible || selectionMenu.visible || fileOptions.visible || pagesPanel.visible
                || geometryOptions.visible || penOptions.visible || penOptions.colorDialogOpen || shapeOptions.visible
                || eraserOptions.visible || backgroundOptions.visible || backgroundOptions.colorDialogOpen
            onCloseRequested: root.closeBrowser()
        }
    }
    Rectangle {
        id: browserDivider; objectName: "browserDivider"
        visible: root.browserOpen
        anchors.top: board.top; anchors.bottom: board.bottom
        x: board.width; width: 6
        color: dividerDrag.containsMouse || dividerDrag.pressed ? Theme.accent : "transparent"
        Rectangle { anchors.centerIn: parent; width: 2; height: Theme.section; radius: 1; color: Theme.border }
        MouseArea {
            id: dividerDrag; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.SplitHCursor
            property real startX: 0
            property real startWidth: 0
            onPressed: mouse => { startX=mapToItem(root,mouse.x,mouse.y).x;startWidth=root.browserWidth;board.cancelStroke() }
            onPositionChanged: mouse => {
                if(pressed)root.browserPreferredWidth=Math.max(300,Math.min(root.width-320,startWidth+startX-mapToItem(root,mouse.x,mouse.y).x))
            }
        }
    }
    BusyIndicator { objectName: "pageImagesLoadingIndicator"; anchors.centerIn: board; visible: App.pageImagesLoading; running: visible }
    PropertiesPanel { id: properties; enabled: !App.constructionPlayback; canvas: board; onExportRequested: svg => root.exportSelection(svg); visible: board.selectedCount > 0 && board.tool === "select" && !inlineText.active; anchors.top: parent.top; anchors.right: board.right; anchors.topMargin: root.headerHeight+Theme.lg; anchors.rightMargin: Theme.lg }
    RowLayout {
        id: topbar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: Theme.lg;anchors.rightMargin: Theme.lg
        height: root.headerRowHeight
        spacing: Theme.sm
        C.IconButton { implicitWidth: 32; implicitHeight: 32; Layout.minimumWidth: implicitWidth; objectName: "editorBackButton"; iconName: "back"; label: "Voltar aos projetos"; enabled: !App.documentBusy; onClicked: root.finishTextThen(function(){ board.cancelStroke();root.backRequested() }) }
        ColumnLayout {
            Layout.preferredWidth: root.width>=1000 ? 120 : 64; Layout.minimumWidth: 48; Layout.maximumWidth: 140; spacing: Theme.xs
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
            Text { text: App.pageInfinite ? "Quadro infinito" : App.pageWidth.toFixed(1) + " × " + App.pageHeight.toFixed(1) + " mm"; color: Theme.secondary; font.pixelSize: Theme.caption; elide: Text.ElideRight; Layout.fillWidth: true }
        }
        ToolOptionsBar {
            id: toolOptions
            enabled: !App.constructionPlayback
            Layout.fillWidth: true; Layout.minimumWidth: 40; Layout.preferredWidth: implicitWidth
            canvas: board; palette: penOptions; textEditor: inlineText
            onCustomColorRequested: penOptions.chooseCustomColor()
        }
        RowLayout {
            id: headerActions
            enabled: !App.constructionPlayback
            objectName: "headerActions"
            Layout.minimumWidth: implicitWidth; Layout.maximumWidth: implicitWidth
            spacing: Theme.sm
        C.ActionButton { implicitHeight: 32; Layout.minimumWidth: implicitWidth; id: fileButton;objectName: "fileMenuButton";text: root.width>=1100 ? "Arquivo" : "";iconName: "folder";enabled: !App.documentBusy;onClicked: root.finishTextThen(function(){board.cancelStroke();root.togglePanel(fileOptions)}) }
        C.ActionButton { implicitHeight: 32; Layout.minimumWidth: implicitWidth; id: pagesButton;objectName: "openPagesButton"; visible: root.width > Theme.zoomBreakpoint; iconName: "pages"; text: root.width>=1400 ? "Páginas" : ""; onClicked: root.finishTextThen(function(){board.cancelStroke();root.togglePanel(pagesPanel)}) }
        C.IconButton { implicitWidth: 32; implicitHeight: 32; Layout.minimumWidth: implicitWidth; iconName: "minus"; label: "Diminuir zoom"; visible: root.width > Theme.zoomBreakpoint; onClicked: board.zoomBy(1/1.2) }
        Item {
            id: zoomControl;objectName: "zoomControl"
            visible: root.width>900
            Layout.minimumWidth: 64; Layout.preferredWidth: 64;Layout.preferredHeight: Theme.controlHeight
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
        C.IconButton { implicitWidth: 32; implicitHeight: 32; Layout.minimumWidth: implicitWidth; iconName: "plus"; label: "Aumentar zoom"; visible: root.width > Theme.zoomBreakpoint; onClicked: board.zoomBy(1.2) }
        C.IconButton { implicitWidth: 32; implicitHeight: 32; Layout.minimumWidth: implicitWidth; iconName: "fit"; label: "Ajustar página"; onClicked: board.fitPage() }
        C.IconButton { implicitWidth: 32; implicitHeight: 32; Layout.minimumWidth: implicitWidth; objectName: "fullScreenButton";iconName: root.fullScreen ? "restoreScreen" : "fullScreen";label: root.fullScreen ? "Sair da tela cheia (F11)" : "Tela cheia (F11)";selected: root.fullScreen;onClicked: root.fullScreenRequested() }
        C.IconButton { implicitWidth: 32; implicitHeight: 32; Layout.minimumWidth: implicitWidth; id: backgroundButton; visible: root.width>=700;objectName: "backgroundOptionsButton";iconName: "background"; label: "Personalizar fundo e grade da página"; onClicked: root.finishTextThen(function(){board.cancelStroke();root.togglePanel(backgroundOptions)}) }
        C.IconButton { implicitWidth: 32; implicitHeight: 32; Layout.minimumWidth: implicitWidth; objectName: "openBrowserButton"; visible: root.width>=800; iconName: "planet"; label: root.browserOpen ? "Fechar navegador lateral" : "Abrir navegador lateral"; selected: root.browserOpen; onClicked: root.finishTextThen(function(){board.cancelStroke();root.browserCreated=true;root.browserOpen=!root.browserOpen;if(!root.browserOpen)board.forceActiveFocus()}) }
        }
    }
    C.GlassPopover {
        id: fileOptions;objectName: "fileOptions"
        width: Theme.propertiesWidth
        onAboutToShow: { const point=fileButton.mapToItem(root,0,fileButton.height+Theme.sm);x=Math.max(Theme.lg,Math.min(point.x,root.width-width-Theme.lg));y=point.y }
        onClosed: board.forceActiveFocus()
        contentItem: ColumnLayout {
            spacing: Theme.xs
            Text { text: "Arquivo";color: Theme.text;font.pixelSize: Theme.body;font.weight: Font.DemiBold; Layout.leftMargin: Theme.sm; Layout.bottomMargin: Theme.sm }
            C.MenuAction { objectName: "newBoardInFolderButton"; text: "Novo quadro"; iconName: "plus"; Layout.fillWidth: true; enabled: !App.documentBusy; onClicked: { fileOptions.close();root.finishNameEdit();root.finishTextThen(function(){board.cancelStroke();App.requestNewInCurrentFolder()}) } }
            C.MenuAction { objectName: "saveBoardAsButton";text: "Salvar como…";iconName: "save";Layout.fillWidth: true;enabled: !App.documentBusy;onClicked: { fileOptions.close();root.saveAsRequested() } }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; Layout.topMargin: Theme.xs; Layout.bottomMargin: Theme.xs; color: Theme.border }
            C.MenuAction { objectName: "importBoardButton";text: "Importar quadro…";iconName: "folder";Layout.fillWidth: true;enabled: !App.documentBusy;onClicked: { fileOptions.close();root.importBoardRequested() } }
            C.MenuAction { objectName: "importPdfButton";text: "Importar PDF…";iconName: "pdfImport";Layout.fillWidth: true;enabled: !App.documentBusy;onClicked: { fileOptions.close();root.importPdfRequested() } }
            C.MenuAction { objectName: "exportPdfButton";text: "Exportar PDF…";iconName: "pdfExport";Layout.fillWidth: true;enabled: !App.documentBusy && !App.exporting;onClicked: { fileOptions.close();root.exportPdfRequested() } }
            C.MenuAction { visible: root.width<700; text: "Fundo da página"; iconName: "background"; Layout.fillWidth: true; onClicked: {fileOptions.close();backgroundOptions.open()} }
            C.MenuAction { visible: root.width<800; text: "Navegador lateral"; iconName: "planet"; Layout.fillWidth: true; onClicked: {fileOptions.close();root.browserCreated=true;root.browserOpen=!root.browserOpen} }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; Layout.topMargin: Theme.sm; Layout.bottomMargin: Theme.sm; color: Theme.border }
            Text { text: "O arquivo .scalar guarda todas as páginas e objetos para abrir em outro computador.";Layout.fillWidth: true;wrapMode: Text.WordWrap;color: Theme.secondary;font.pixelSize: Theme.caption }
        }
    }
    FloatingToolbar {
        id: toolbar
        width: Math.min(implicitWidth,Math.max(0,board.width-Theme.xxl))
        backdropSource: board
        anchors.horizontalCenter: board.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: board.width < toolbar.width+2*navigator.width+Theme.lg*3 ? navigator.height+Theme.lg+Theme.md : Theme.xxl
        hasRecording: root.recorder.count>0
        recording: root.recorder.recording
        recordingEnabled: !App.documentBusy && !root.recorder.exporting && !root.recorder.pending && !App.constructionPlayback || root.recorder.recording
        onRecordingRequested: root.finishTextThen(function(){
            board.cancelStroke()
            if(root.recorder.recording)root.recorder.stop()
            else {
                const center=board.viewportCenter(),scale=board.zoom*96/25.4
                App.startConstructionRecording(Qt.rect(center.x-board.width/(2*scale),center.y-board.height/(2*scale),board.width/scale,board.height/scale))
            }
        })
        geometryMagnet: board.geometryMagnet
        onMagnetToggled: {board.geometryMagnet=!board.geometryMagnet;board.forceActiveFocus()}
        activeTool: board.tool
        penColor: board.penColor
        markerColor: board.markerColor
        onToolSelected: tool => root.finishTextThen(function(){ root.recorder.closePreview();board.tool=tool;board.forceActiveFocus() })
        onPenOptionsRequested: root.togglePanel(penOptions)
        onShapeOptionsRequested: root.togglePanel(shapeOptions)
        onEraserOptionsRequested: root.togglePanel(eraserOptions)
        onImageRequested: root.imageRequested()
        onGeometryOptionsRequested: root.togglePanel(geometryOptions)
    }
    C.ConstructionTimeline {
        id: constructionTimeline
        recorder: root.recorder
        enabled: !App.documentBusy
        anchors.horizontalCenter: board.horizontalCenter
        anchors.bottom: toolbar.top; anchors.bottomMargin: Theme.sm
        width: Math.min(760,board.width-Theme.lg*2)
        onExportRequested: format => {constructionExport.format=format;constructionExport.open()}
    }
    FileDialog {
        id: constructionExport
        property string format: "mp4"
        title: "Exportar construção"
        fileMode: FileDialog.SaveFile
        defaultSuffix: format
        nameFilters: format==="gif" ? ["Animação GIF (*.gif)"] : ["Vídeo MP4 (*.mp4)"]
        onAccepted: root.recorder.exportAnimation(selectedFile,format)
    }
    C.PageNavigator {
        id: navigator
        objectName: "pageNavigator"
        enabled: !App.constructionPlayback
        x: Math.max(Theme.lg,board.width-width-Theme.lg)
        y: root.height-height-Theme.lg
        onPreviousRequested: root.finishTextThen(function(){board.cancelStroke();App.selectPage(App.currentPage-1);board.forceActiveFocus()})
        onNextRequested: root.finishTextThen(function(){board.cancelStroke();App.selectPage(App.currentPage+1);board.forceActiveFocus()})
        onAddRequested: root.finishTextThen(function(){board.cancelStroke();App.addPage();board.forceActiveFocus()})
        onOverviewRequested: root.finishTextThen(function(){board.cancelStroke();root.togglePanel(pagesPanel)})
    }
    C.InlineTextEditor {
        id: inlineText; anchors.fill: board; canvas: board
        onFinished: { const action=root.afterTextEdit;root.afterTextEdit=null;if(action)action() }
    }
    GeometryOptions {
        id: geometryOptions; canvas: board
        maximumHeight: Math.max(Theme.section*3,toolbar.y-root.headerHeight-Theme.lg*3)
        function reposition() { x=Math.max(Theme.lg,(board.width-width)/2); y=Math.max(Theme.lg,toolbar.y-height-Theme.md) }
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
        function reposition() { x = Math.max(Theme.lg,(board.width-width)/2); y = Math.max(Theme.lg,toolbar.y-height-Theme.md) }
        onAboutToShow: reposition()
        onHeightChanged: Qt.callLater(reposition)
        Connections {
            target: root
            function onWidthChanged() { penOptions.reposition() }
            function onHeightChanged() { Qt.callLater(penOptions.reposition) }
        }
    }
    ShapeOptions { id: shapeOptions; canvas: board; x: Math.max(Theme.lg, (board.width-width)/2); y: Math.max(Theme.lg,toolbar.y-height-Theme.md) }
    C.GlassPopover {
        id: eraserOptions; objectName: "eraserOptions"; x: Math.max(Theme.lg,(board.width-width)/2); y: toolbar.y-height-Theme.md
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
        width: Math.min(implicitWidth, board.width-Theme.xxl)
        text: board.drawing ? board.interactionHint : App.status
        color: Theme.secondary; font.pixelSize: Theme.caption; elide: Text.ElideRight
    }
    Shortcut { sequence: "E"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "eraser" }
    readonly property bool penColorShortcutsEnabled: {
        const focus=root.Window.window ? root.Window.window.activeFocusItem : null
        const typing=focus && (focus.hasOwnProperty("textDocument") || focus.hasOwnProperty("acceptableInput"))
        return !typing && !inlineText.active && !projectTitle.editing && !zoomControl.editing && !root.overlaysOpen
            && !App.documentBusy && !board.drawing && !selectionExport.visible && !selectionMenu.visible
            && !fileOptions.visible && !pagesPanel.visible && !geometryOptions.visible && !shapeOptions.visible
            && !eraserOptions.visible && !backgroundOptions.visible && !backgroundOptions.colorDialogOpen && !penOptions.colorDialogOpen
    }
    Repeater {
        model: 7
        delegate: Item {
            required property int index
            Shortcut { sequence: String(index+1); context: Qt.ApplicationShortcut; enabled: !App.constructionPlayback && !root.browserFocused && root.penColorShortcutsEnabled; onActivated: penOptions.pickColor(index) }
        }
    }
    readonly property bool eraserSizeShortcutsEnabled: board.tool === "eraser" && (board.enabled || eraserOptions.visible) && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !projectTitle.editing && !zoomControl.editing
    Shortcut { sequences: ["+", "]"]; enabled: !App.constructionPlayback && !root.browserFocused && root.eraserSizeShortcutsEnabled; autoRepeat: true; onActivated: board.adjustEraserSize(true) }
    Shortcut { sequences: ["-", "["]; enabled: !App.constructionPlayback && !root.browserFocused && root.eraserSizeShortcutsEnabled; autoRepeat: true; onActivated: board.adjustEraserSize(false) }
    Shortcut { sequence: "Ctrl+C"; enabled: !App.constructionPlayback && !root.browserFocused && board.enabled && !projectTitle.editing && !zoomControl.editing && board.selectedCount > 0; onActivated: board.copySelection() }
    Shortcut { sequence: "Ctrl+V"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !projectTitle.editing && !zoomControl.editing; onActivated: board.pasteSelection() }
    Shortcut { sequence: "V"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "select" }
    Shortcut { sequence: "Delete"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && (board.selectedCount > 0 || board.selectedGuide !== ""); onActivated: board.deleteSelection() }
    Shortcut { sequence: "Ctrl+D"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && board.selectedCount > 0; onActivated: board.duplicateSelection() }
    Shortcut { sequence: "Ctrl+Up"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && board.selectedCount > 0; onActivated: board.moveSelectionLayer(true) }
    Shortcut { sequence: "Ctrl+Down"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && board.selectedCount > 0; onActivated: board.moveSelectionLayer(false) }
    Shortcut { sequence: "T"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "text" }
    Shortcut { sequence: "P"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "pen" }
    Shortcut { sequence: "M"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: board.tool = "marker" }
    Shortcut { sequence: "Ctrl+Z"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !board.drawing; onActivated: App.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active && !board.drawing; onActivated: App.redo() }
    Shortcut { sequence: "Ctrl+S"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible; onActivated: root.finishTextThen(function(){App.save()}) }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: root.saveAsRequested() }
    Shortcut { sequence: "Ctrl+PgDown"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: { board.cancelStroke(); App.selectPage(App.currentPage+1) } }
    Shortcut { sequence: "Ctrl+PgUp"; enabled: !App.constructionPlayback && !root.browserFocused && !root.overlaysOpen && !selectionExport.visible && !selectionMenu.visible && !inlineText.active; onActivated: { board.cancelStroke(); App.selectPage(App.currentPage-1) } }
}
