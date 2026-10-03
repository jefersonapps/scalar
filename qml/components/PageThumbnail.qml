import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
AbstractButton {
    id: root
    property var pageData
    property bool selected: false
    implicitHeight: Theme.pageTileHeight
    hoverEnabled: true
    Accessible.name: "Página " + (pageData.index+1) + (pageData.pdf ? " · PDF" : "")
    Accessible.checked: selected
    background: Rectangle { radius: Theme.radiusMedium; color: root.selected ? Theme.accentSoft : root.hovered ? Theme.hover : Theme.surface; border.width: root.selected ? Theme.focusBorder : Theme.hairline; border.color: root.selected ? Theme.accent : Theme.border }
    contentItem: ColumnLayout {
        spacing: Theme.xs
        Image { Layout.fillWidth: true; Layout.fillHeight: true; source: root.pageData.thumbnail; fillMode: Image.PreserveAspectFit; cache: false }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Página " + (root.pageData.index+1); color: Theme.text; font.pixelSize: Theme.caption; Layout.fillWidth: true }
            Icon { visible: root.selected; name: "check"; color: Theme.accent; width: Theme.lg; height: Theme.lg }
        }
        Text { text: root.pageData.widthMm.toFixed(0) + " × " + root.pageData.heightMm.toFixed(0) + " mm" + (root.pageData.pdf ? " · PDF" : ""); color: Theme.secondary; font.pixelSize: Theme.caption }
    }
    padding: Theme.sm
}
