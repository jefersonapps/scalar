import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPopover {
    id: root
    objectName: "pagesPanel"
    width: Math.min(Theme.pagesPanelWidth,parent.width-Theme.xxl)
    signal importPdfRequested()
    contentItem: ColumnLayout {
        spacing: Theme.md
        RowLayout {
            Text { text: "Páginas · " + App.pageCount; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold; Layout.fillWidth: true }
            C.IconButton { compact: true; iconName: "close"; label: "Fechar páginas"; onClicked: root.close() }
        }
        GridView {
            id: grid
            Layout.fillWidth: true; Layout.preferredHeight: Math.min(Theme.pageTileHeight*2+Theme.lg,root.parent.height-Theme.topbarHeight-Theme.section*4)
            clip: true; cellWidth: width/2; cellHeight: Theme.pageTileHeight+Theme.sm
            model: App.pages
            delegate: C.PageThumbnail {
                required property var modelData
                objectName: "pageThumbnail_" + modelData.index
                width: grid.cellWidth-Theme.sm; height: Theme.pageTileHeight
                pageData: modelData; selected: modelData.index === App.currentPage; enabled: !App.busy
                onClicked: App.selectPage(modelData.index)
            }
            ScrollBar.vertical: ScrollBar {}
        }
        RowLayout {
            C.ActionButton { objectName: "addPageButton"; text: "Nova página"; Layout.fillWidth: true; enabled: !App.busy && App.pageCount < 1000; onClicked: App.addPage() }
            C.IconButton { iconName: "duplicate"; label: "Duplicar página atual"; enabled: !App.busy && App.pageCount < 1000; onClicked: App.duplicatePage() }
        }
        C.ActionButton { objectName: "importPdfButton"; text: "Importar PDF"; Layout.fillWidth: true; enabled: !App.busy; onClicked: { root.close(); root.importPdfRequested() } }
    }
}
