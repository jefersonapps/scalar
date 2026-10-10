import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPopover {
    id: root
    objectName: "pagesPanel"
    width: Math.min(Theme.pagesPanelWidth,parent.width-Theme.xxl)
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
                pageData: modelData; selected: modelData.index === App.currentPage; enabled: !App.documentBusy
                onClicked: App.selectPage(modelData.index)
                C.IconButton {
                    objectName: "deletePageButton_" + modelData.index
                    z: 2
                    anchors.top: parent.top; anchors.right: parent.right; anchors.margins: Theme.xs
                    compact: true; iconName: "trash"; label: "Excluir página " + (modelData.index + 1)
                    implicitWidth: 26; implicitHeight: 26; padding: 5
                    contentItem: C.Icon { name: "trash"; implicitWidth: 16; implicitHeight: 16; color: Theme.text }
                    background: Rectangle {
                        radius: Theme.radiusMedium
                        color: parent.down ? Theme.pressed : parent.hovered ? Theme.hover : Theme.surface
                        border.width: Theme.hairline; border.color: Theme.border
                    }
                    onClicked: App.deletePage(modelData.index)
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
        RowLayout {
            C.ActionButton { objectName: "addPageButton"; text: "Nova página"; iconName: "pageAdd"; Layout.fillWidth: true; enabled: !App.documentBusy && App.pageCount < 1000; onClicked: App.addPage() }
            C.ActionButton { text: "Duplicar"; iconName: "duplicate"; enabled: !App.documentBusy && App.pageCount < 1000; onClicked: App.duplicatePage() }
        }
    }
}
