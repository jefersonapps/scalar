import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
AbstractButton {
    id: root
    property string fontFamily: App.defaultTextFont
    signal picked(string family)
    readonly property bool expanded: popup.visible
    implicitHeight: Theme.controlHeight
    implicitWidth: Theme.propertiesWidth
    hoverEnabled: true
    FontMetrics { id: scrollMetrics;font.pixelSize: Theme.body }
    Accessible.name: "Escolher fonte"
    function familyLabel(family) { return family.replace(/\s*\[[^\]]*\]/g,"").trim() }
    readonly property var entries: {
        const unique={};const result=[]
        for(const family of App.fontFamilies){
            const label=familyLabel(family)
            if(unique[label]===undefined){unique[label]=result.length;result.push({label:label,family:family})}
            else if(family===label)result[unique[label]].family=family
        }
        return result
    }
    readonly property var filteredEntries: {
        const query=search.text.trim().toLocaleLowerCase()
        return entries.filter(entry=>entry.label.toLocaleLowerCase().indexOf(query)>=0)
    }
    contentItem: RowLayout {
        spacing: Theme.md
        Text { Layout.fillWidth: true;Layout.fillHeight: true;text: root.familyLabel(root.fontFamily);color: Theme.text;font.pixelSize: Theme.body;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter }
        Icon { name: "chevron";color: Theme.text }
    }
    leftPadding: Theme.lg;rightPadding: Theme.lg
    topPadding: 0;bottomPadding: 0
    background: Rectangle { color: root.hovered ? Theme.hover : Theme.background;radius: Theme.radiusMedium;border.color: popup.visible ? Theme.accent : Theme.border }
    onClicked: popup.open()
    Popup {
        id: popup;objectName: "fontPickerPopup"
        parent: Overlay.overlay
        width: root.width
        height: Math.min(Theme.touch*9,root.Window.window ? root.Window.window.height-Theme.xxl*2 : Theme.touch*9)
        padding: Theme.md;margins: Theme.lg;focus: true
        modal: true;dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: GlassPanel { color: Theme.dropdownSurface;border.color: Theme.dropdownBorder;radius: Theme.radiusLarge }
        onAboutToShow: {
            const position=root.mapToItem(Overlay.overlay,0,root.height+Theme.xs)
            x=position.x;y=position.y
        }
        onOpened: { search.clear();list.currentIndex=-1;list.positionViewAtBeginning();search.forceActiveFocus() }
        contentItem: ColumnLayout {
            spacing: Theme.sm
            Field {
                id: search;objectName: "fontSearchField"
                Layout.fillWidth: true;placeholderText: "Buscar fonte"
                onTextChanged: { list.currentIndex=-1;list.positionViewAtBeginning() }
                Keys.onDownPressed: { if(list.count){list.currentIndex=0;list.forceActiveFocus()} }
                onAccepted: if(root.filteredEntries.length){root.picked(root.filteredEntries[0].family);popup.close()}
            }
            Text {
                Layout.fillWidth: true
                text: root.filteredEntries.length+" de "+root.entries.length+" fontes"
                color: Theme.secondary;font.pixelSize: Theme.caption
            }
            ScrollView {
                id: fontScrollView
                Layout.fillWidth: true;Layout.fillHeight: true
                clip: true
                wheelEnabled: false
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical: ScrollBar {
                    objectName: "fontScrollBar"
                    parent: fontScrollView
                    x: fontScrollView.width-width
                    y: fontScrollView.topPadding
                    height: fontScrollView.availableHeight
                    policy: ScrollBar.AlwaysOn;minimumSize: 0.05
                    contentItem: Rectangle { implicitWidth: Theme.sm;implicitHeight: Theme.touch;radius: Theme.sm/2;color: Theme.secondary }
                    background: Rectangle { radius: Theme.sm/2;color: Theme.hover }
                }
                contentItem: ListView {
                    id: list;objectName: "fontList"
                    clip: true;model: root.filteredEntries;keyNavigationEnabled: true
                    boundsBehavior: Flickable.StopAtBounds
                    reuseItems: true
                    cacheBuffer: Theme.touch*2
                    // Mouse wheels scroll by the system's line setting, without a flick.
                    // Trackpads retain their precise pixel deltas.
                    WheelHandler {
                        target: null
                        onWheel: event => {
                            list.cancelFlick()
                            const delta=event.pixelDelta.y !== 0 ? event.pixelDelta.y
                                : event.angleDelta.y/120*Qt.styleHints.wheelScrollLines*scrollMetrics.height
                            list.contentY=Math.max(0,Math.min(Math.max(0,list.contentHeight-list.height),list.contentY-delta))
                            event.accepted=true
                        }
                    }
                    delegate: ItemDelegate {
                        id: option
                        required property var modelData
                        required property int index
                        objectName: "fontOption_"+index
                        width: list.width-Theme.lg;height: Theme.touch+Theme.xl
                        padding: Theme.md
                        hoverEnabled: true
                        clip: true
                        highlighted: ListView.isCurrentItem
                        contentItem: Item {
                            implicitHeight: Theme.xl+Theme.xs+Theme.lg
                            Item {
                                anchors.left: parent.left;anchors.right: parent.right;anchors.rightMargin: Theme.icon+Theme.md
                                height: parent.height
                                Text {
                                    objectName: "fontPreview_"+option.index
                                    width: parent.width;height: Theme.xl;clip: true
                                    text: option.modelData.label;color: Theme.text
                                    font: App.textFont(option.modelData.family,Theme.heading)
                                    elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    y: Theme.xl+Theme.xs;width: parent.width;height: Theme.lg;clip: true
                                    text: option.modelData.label;color: Theme.secondary;font.pixelSize: Theme.caption
                                    elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter
                                }
                            }
                            Icon { anchors.right: parent.right;anchors.verticalCenter: parent.verticalCenter;name: "check";color: Theme.accent;visible: root.familyLabel(root.fontFamily)===option.modelData.label }
                        }
                        background: Rectangle { objectName: "fontOptionBackground_"+option.index;color: option.hovered ? Theme.dropdownHover : option.highlighted || root.familyLabel(root.fontFamily)===option.modelData.label ? Theme.accentSoft : "transparent";radius: Theme.radiusSmall }
                        onClicked: { root.picked(modelData.family);popup.close() }
                    }
                    Keys.onReturnPressed: if(currentIndex>=0){root.picked(root.filteredEntries[currentIndex].family);popup.close()}
                    Text { anchors.centerIn: parent;visible: list.count===0;text: "Nenhuma fonte encontrada";color: Theme.secondary;font.pixelSize: Theme.caption }
                }
            }
        }
    }
}
