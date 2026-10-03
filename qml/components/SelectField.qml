import QtQuick
import QtQuick.Controls
import "../theme"
ComboBox {
    id: root
    implicitHeight: Theme.touch
    font.pixelSize: Theme.body
    leftPadding: Theme.lg
    rightPadding: Theme.section
    contentItem: Text { text: root.displayText; color: Theme.text; font: root.font; verticalAlignment: Text.AlignVCenter }
    indicator: Icon { name: "chevron"; x: root.width-width-Theme.md; y: (root.height-height)/2 }
    background: Rectangle { color: Theme.background; radius: Theme.radiusMedium; border.color: root.activeFocus ? Theme.accent : Theme.border }
    delegate: ItemDelegate {
        width: root.width; height: Theme.touch
        contentItem: Text { text: modelData; color: Theme.text; font.pixelSize: Theme.body; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: highlighted ? Theme.accentSoft : Theme.surface; radius: Theme.radiusSmall }
        highlighted: root.highlightedIndex === index
    }
    popup: Popup {
        y: root.height+Theme.xs; width: root.width; padding: Theme.sm
        implicitHeight: Math.min(contentItem.implicitHeight+Theme.lg,Theme.touch*6)
        background: GlassPanel { radius: Theme.radiusLarge }
        contentItem: ListView { clip: true; implicitHeight: contentHeight; model: root.popup.visible ? root.delegateModel : null; currentIndex: root.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
    }
}
