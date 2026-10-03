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
    function change(key, value) { const next = Object.assign({}, draft); next[key] = value; draft = next; applyLive() }
    function applyLive() { if(App.validBackground(draft)) App.setBackground(draft) }
    function chooseGrid(index) {
        if(index < 0 || index >= App.gridPresets.length) return
        const preset = App.gridPresets[index]
        draft = Object.assign({},draft,{gridType:preset.gridType,spacingX:preset.spacingX,spacingY:preset.spacingY})
        applyLive()
    }
    onOpened: draft = Object.assign({}, App.background)
    contentItem: ScrollView {
        id: backgroundScroll
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        implicitWidth: Theme.propertiesWidth
        implicitHeight: Math.min(form.implicitHeight,root.parent.height-Theme.topbarHeight-Theme.xxl)
        clip: true
        ColumnLayout {
        id: form
        width: backgroundScroll.availableWidth
        spacing: Theme.sm
        Text { text: "Fundo e grade"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold }
        C.SelectField {
            objectName: "gridPresetSelector"
            Layout.fillWidth: true; model: App.gridPresets.map(p => p.name).concat(["Personalizado"])
            currentIndex: {
                const index = App.gridPresets.findIndex(p => p.gridType === root.draft.gridType && p.spacingX === root.draft.spacingX && p.spacingY === root.draft.spacingY)
                return index < 0 ? App.gridPresets.length : index
            }
            onActivated: root.chooseGrid(index)
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
            currentIndex: root.draft.gridType || 0; onActivated: root.change("gridType",index)
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
