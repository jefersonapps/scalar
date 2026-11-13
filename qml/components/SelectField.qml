import QtQuick
import QtQuick.Controls
import "../theme"
ComboBox {
    id: root
    implicitHeight: Theme.controlHeight
    font.pixelSize: Theme.body
    leftPadding: Theme.lg
    rightPadding: Theme.lg+Theme.icon+Theme.sm
    contentItem: Text { text: root.displayText; color: Theme.text; font: root.font; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
    indicator: Icon { name: "chevron"; x: root.width-width-Theme.lg; y: (root.height-height)/2 }
    background: Rectangle { color: Theme.background; radius: Theme.radiusMedium; border.color: root.activeFocus ? Theme.accent : Theme.border }
    delegate: ItemDelegate {
        width: ListView.view ? ListView.view.width : root.popup.availableWidth
        height: Theme.controlHeight
        leftPadding: Theme.lg; rightPadding: Theme.lg
        hoverEnabled: true
        contentItem: Text { text: modelData; color: Theme.text; font.pixelSize: Theme.body; rightPadding: Theme.icon+Theme.sm; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { color: down ? Theme.pressed : hovered || highlighted ? Theme.hover : "transparent"; radius: Theme.radiusSmall }
        Icon { anchors.right: parent.right; anchors.rightMargin: Theme.lg; anchors.verticalCenter: parent.verticalCenter; name: "check"; color: Theme.accent; visible: index===root.currentIndex }
        highlighted: root.highlightedIndex === index
    }
    popup: GlassPopover {
        y: root.height+Theme.xs; width: Math.max(root.width,180); padding: Theme.sm
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        implicitHeight: Math.min(contentItem.implicitHeight+Theme.lg,Theme.controlHeight*6)
        contentItem: ListView { clip: true; implicitHeight: contentHeight; model: root.popup.visible ? root.delegateModel : null; currentIndex: root.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
    }
}
