import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.GlassPopover {
    id: root
    objectName: "backgroundOptions"
    closePolicy: colorDialogOpen ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    readonly property bool colorDialogOpen: customColor.visible
    property string colorTarget: "color"
    C.CustomColorDialog { id: customColor; onChosen: value => root.change(root.colorTarget,value.toString()); onPreviewed: value => root.change(root.colorTarget,value.toString()) }
    property var draft: ({})
    signal pageSizeChanged()
    function applyPageSize(landscape) {
        if(pagePreset.currentIndex===2 && (!pageWidth.acceptableInput || !pageHeight.acceptableInput))return
        if(App.setPageSize(pagePreset.currentText,Number(pageWidth.text),Number(pageHeight.text),landscape)){
            pageWidth.text=App.pageWidth.toFixed(2);pageHeight.text=App.pageHeight.toFixed(2)
            root.pageSizeChanged()
        }
    }
    function change(key, value) { const next = Object.assign({}, draft); next[key] = value; draft = next; applyLive() }
    function applyLive() { if(App.validBackground(draft)) App.setBackground(draft) }
    function chooseGrid(index) {
        if(index < 0 || index >= App.gridPresets.length) return
        const preset = App.gridPresets[index]
        draft = Object.assign({},draft,{gridType:preset.gridType,spacingX:preset.spacingX,spacingY:preset.spacingY})
        applyLive()
    }
    onOpened: {
        draft=Object.assign({},App.background)
        const w=Math.min(App.pageWidth,App.pageHeight),h=Math.max(App.pageWidth,App.pageHeight)
        pagePreset.currentIndex=App.pageInfinite ? 3 : Math.abs(w-210)<0.01 && Math.abs(h-297)<0.01 ? 0 : Math.abs(w-215.9)<0.01 && Math.abs(h-279.4)<0.01 ? 1 : 2
        pageWidth.text=App.pageWidth.toFixed(2);pageHeight.text=App.pageHeight.toFixed(2)
    }
    contentItem: ScrollView {
        id: backgroundScroll
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        implicitWidth: Theme.propertiesWidth
        implicitHeight: Math.min(form.implicitHeight,root.parent.height-root.y-root.topPadding-root.bottomPadding-Theme.lg)
        clip: true
        ColumnLayout {
        id: form
        width: backgroundScroll.availableWidth
        spacing: Theme.sm
        Text { text: "Fundo e grade"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
        Text { text: "Tamanho da página atual";color: Theme.secondary;font.pixelSize: Theme.caption }
        C.SelectField {
            id: pagePreset;objectName: "currentPageSizeSelector"
            Layout.fillWidth: true;model: ["A4","Carta","Personalizado","Infinito"]
            onActivated: index => { if(index!==2)root.applyPageSize(App.pageWidth>App.pageHeight) }
        }
        RowLayout {
            visible: pagePreset.currentIndex===2
            C.Field { id: pageWidth;objectName: "currentPageWidthField";Layout.fillWidth: true;placeholderText: "Largura (mm)";validator: DoubleValidator { bottom: 10;top: 5000;locale: "C";decimals: 2 } onAccepted: root.applyPageSize(App.pageWidth>App.pageHeight) }
            Text { text: "×";color: Theme.secondary }
            C.Field { id: pageHeight;objectName: "currentPageHeightField";Layout.fillWidth: true;placeholderText: "Altura (mm)";validator: DoubleValidator { bottom: 10;top: 5000;locale: "C";decimals: 2 } onAccepted: root.applyPageSize(App.pageWidth>App.pageHeight) }
            Text { text: "mm";color: Theme.secondary;font.pixelSize: Theme.caption }
        }
        C.ActionButton { visible: pagePreset.currentIndex===2;text: "Aplicar tamanho";iconName: "page";Layout.fillWidth: true;enabled: pageWidth.acceptableInput && pageHeight.acceptableInput && !App.documentBusy;onClicked: root.applyPageSize(App.pageWidth>App.pageHeight) }
        C.SegmentedControl {
            objectName: "currentPageOrientation"
            visible: !App.pageInfinite
            Layout.fillWidth: true;options: ["Retrato","Paisagem"]
            currentIndex: App.pageWidth>App.pageHeight ? 1 : 0
            onSelected: index => root.applyPageSize(index===1)
        }
        Text { Layout.fillWidth: true;wrapMode: Text.WordWrap;text: App.pageInfinite ? "Espaço ilimitado · PDF ajustado ao conteúdo" : App.pageWidth.toFixed(1)+" × "+App.pageHeight.toFixed(1)+" mm";color: Theme.secondary;font.pixelSize: Theme.caption }
        Rectangle { Layout.fillWidth: true;height: Theme.hairline;color: Theme.border;Layout.topMargin: Theme.sm;Layout.bottomMargin: Theme.sm }
        Text { text: "Grade";color: Theme.secondary;font.pixelSize: Theme.caption }
        C.SelectField {
            objectName: "gridPresetSelector"
            Layout.fillWidth: true; model: App.gridPresets.map(p => p.name).concat(["Personalizado"])
            currentIndex: {
                const index = App.gridPresets.findIndex(p => p.gridType === root.draft.gridType && p.spacingX === root.draft.spacingX && p.spacingY === root.draft.spacingY)
                return index < 0 ? App.gridPresets.length : index
            }
            onActivated: index => root.chooseGrid(index)
        }
        Text { text: "Cor do fundo"; color: Theme.secondary; font.pixelSize: Theme.caption }
        Flow {
            Layout.fillWidth: true; Layout.preferredHeight: childrenRect.height; spacing: Theme.xs
            Repeater {
                model: Theme.backgroundColors
                C.ColorButton { required property string modelData; required property int index; objectName: "backgroundColor_" + index; swatch: modelData; selected: root.draft.color === modelData; onClicked: root.change("color",swatch.toString()) }
            }
        }
        RowLayout {
            C.Field { Layout.fillWidth: true; text: root.draft.color || "#ffffff"; onTextEdited: root.change("color",text); placeholderText: "#ffffff" }
            C.IconButton { objectName: "customBackgroundColorButton"; iconName: "palette"; label: "Cor personalizada do fundo"; onClicked: { root.colorTarget = "color"; customColor.choose(root.draft.color) } }
        }
        C.SelectField {
            Layout.fillWidth: true; model: ["Sem grade", "Pautado", "Quadriculado", "Pontilhado", "Milimetrado", "Isométrico"]
            currentIndex: root.draft.gridType || 0; onActivated: index => root.change("gridType",index)
        }
        RowLayout {
            visible: root.draft.gridType > 0
            Text { text: "Cor da grade"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true }
            Text { text: "Borda (mm)"; color: Theme.secondary; font.pixelSize: Theme.caption }
        }
        RowLayout {
            visible: root.draft.gridType > 0
            C.IconButton { iconName: "palette"; label: "Cor personalizada da grade"; onClicked: { root.colorTarget = "gridColor"; customColor.choose(root.draft.gridColor) } }
            C.Field { Layout.fillWidth: true; text: root.draft.gridColor || "#64748b"; placeholderText: "Cor da grade"; onTextEdited: root.change("gridColor",text) }
            C.Field { Layout.preferredWidth: Theme.section*2; text: root.draft.thicknessMm || 0.15; validator: DoubleValidator { bottom: 0.02; top: 2; locale: "en_US" } onTextEdited: if(acceptableInput) root.change("thicknessMm",Number(text)) }
        }
        Text { visible: root.draft.gridType > 0; text: "Espaçamento X / Y (mm)"; color: Theme.secondary; font.pixelSize: Theme.caption }
        RowLayout {
            visible: root.draft.gridType > 0
            Repeater {
                model: ["spacingX", "spacingY"]
                C.Field { required property string modelData; Layout.fillWidth: true; text: root.draft[modelData] || 5; validator: DoubleValidator { bottom: 1; top: 100; locale: "en_US" } onTextEdited: if(acceptableInput) root.change(modelData,Number(text)) }
            }
        }
        Text { visible: root.draft.gridType > 0; text: "Opacidade da grade"; color: Theme.secondary; font.pixelSize: Theme.caption }
        C.ModernSlider { visible: root.draft.gridType > 0; Layout.fillWidth: true; from: 0; to: 1; value: root.draft.opacity || 0; onMoved: root.change("opacity",value) }
        RowLayout {
            C.Field { id: presetName; Layout.fillWidth: true; placeholderText: "Nome do preset" }
            C.ActionButton { text: "Salvar"; enabled: presetName.text.trim().length > 0 && App.validBackground(root.draft); onClicked: App.saveBackgroundPreset(presetName.text,root.draft) }
        }
        Text { Layout.fillWidth: true; text: "Alterações aplicadas em tempo real"; color: Theme.secondary; font.pixelSize: Theme.caption; wrapMode: Text.WordWrap }
    }
    }
}
