import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
RowLayout {
    id: root
    property var editor
    readonly property bool popupOpen: fonts.expanded || alignment.popup.visible || customColor.visible
    property var canvas
    readonly property string selectedId: canvas.selectedCount===1 && canvas.selectionName==="Texto" ? canvas.selectedTextId() : ""
    readonly property var values: editor.active ? editor.draft : selectedId ? App.textValues(selectedId) : editor.defaults
    readonly property var emphasis: editor.active ? editor.selectionStyle : values
    function apply(action) {
        if(!editor.active && selectedId){editor.begin(Qt.rect(0,0,60,18),selectedId);Qt.callLater(action)}
        else action()
    }
    function change(key,value) {
        apply(function(){
            if(editor.active){editor.change(key,value);editor.focusText()}
            else {const next=Object.assign({},editor.defaults);next[key]=value;editor.defaults=next;if(key==="latexEnabled")App.sessionLatexEnabled=value}
        })
    }
    function color(value) {
        apply(function(){if(editor.active)editor.applyColor(value);else change("color",value.toString())})
    }
    objectName: "inlineTextControls"
    enabled: !editor.committing
    spacing: Theme.xs
    FontSelector {
        id: fonts
        objectName: "toolbarTextFont"
        implicitWidth: 148; implicitHeight: 32; Layout.maximumHeight: 32
        fontFamily: root.values.fontFamily || App.defaultTextFont
        onPicked: family => root.change("fontFamily",family)
    }
    SelectField {
        id: fontSize
        objectName: "toolbarTextSize"
        implicitWidth: 82; implicitHeight: 32; Layout.maximumHeight: 32
        editable: true
        model: [6,8,9,10,11,12,14,16,18,20,22,24,26,28,32,36,48,72,96,144]
        currentIndex: model.indexOf(Number(root.values.fontSizePt || 16))
        editText: String(root.values.fontSizePt || 16)
        Accessible.name: "Tamanho da fonte em pontos"
        validator: DoubleValidator { bottom: 6; top: 144; locale: "C" }
        contentItem: TextInput {
            text: fontSize.editText; color: Theme.text; font.pixelSize: Theme.body
            verticalAlignment: TextInput.AlignVCenter
            selectByMouse: true; validator: fontSize.validator
            onTextEdited: fontSize.editText=text
            onEditingFinished: if(acceptableInput)root.change("fontSizePt",Number(text))
        }
        onActivated: root.change("fontSizePt",Number(currentText))
        onAccepted: if(acceptableInput)root.change("fontSizePt",Number(editText))
    }
    Repeater {
        model: ["B","I"]
        IconButton {
            required property string modelData
            objectName: modelData==="B" ? "inlineBoldButton" : "inlineItalicButton"
            implicitWidth: 32; implicitHeight: 32; Layout.maximumHeight: 32
            iconName: modelData==="B" ? "bold" : "italic"
            label: modelData==="B" ? "Negrito (Ctrl+B)" : "Itálico (Ctrl+I)"
            focusPolicy: Qt.NoFocus
            selected: (modelData==="B" ? root.emphasis.bold : root.emphasis.italic) || false
            Accessible.checkable: true
            Accessible.checked: selected
            onClicked: root.apply(function(){if(root.editor.active)root.editor.toggleEmphasis(modelData==="B");else root.change(modelData==="B" ? "bold" : "italic",!selected)})
        }
    }
    ActionButton {
        objectName: "inlineLatexButton"
        implicitWidth: 58; implicitHeight: 32; Layout.maximumHeight: 32
        text: "LaTeX"; focusPolicy: Qt.NoFocus
        checked: root.editor.active || root.selectedId ? root.values.latexEnabled!==false : App.sessionLatexEnabled
        onClicked: root.change("latexEnabled",!checked)
    }
    SelectField {
        id: alignment
        objectName: "toolbarTextAlignment"
        implicitWidth: 114; implicitHeight: 32; Layout.maximumHeight: 32
        model: ["Esquerda","Centro","Direita"]
        currentIndex: root.values.alignment || 0
        Accessible.name: "Alinhamento do texto"
        onActivated: index => root.change("alignment",index)
    }
    SelectField {
        objectName: "toolbarTextLineSpacing"
        implicitWidth: 82; implicitHeight: 32; Layout.maximumHeight: 32
        model: ["1","1,15","1,5","2","2,5","3"]
        property var factors: [1,1.15,1.5,2,2.5,3]
        currentIndex: factors.indexOf(Number(root.values.lineSpacing || 1))
        Accessible.name: "Espaçamento entre linhas"
        ToolTip.visible: hovered
        ToolTip.text: "Espaçamento entre linhas"
        ToolTip.delay: Theme.tooltipDelay
        onActivated: index => root.change("lineSpacing",factors[index])
    }
    Repeater {
        model: [App.pageColor.hslLightness<0.5 ? Theme.inkNeutralOnDark : Theme.inkNeutralOnLight].concat(Theme.inkHues.map((h,i)=>Qt.hsla(h,Theme.inkSaturations[i],App.pageColor.hslLightness<0.5 ? Theme.inkLightnessOnDark[i] : Theme.inkLightnessOnLight[i],1)))
        ColorButton {
            required property var modelData
            required property int index
            objectName: "inlineTextColor_"+index
            implicitWidth: 30; implicitHeight: 32; Layout.maximumHeight: 32
            swatch: modelData; selected: Qt.colorEqual(root.emphasis.color || root.values.color || root.canvas.penColor,swatch)
            focusPolicy: Qt.NoFocus
            onClicked: root.color(swatch)
        }
    }
    IconButton {
        objectName: "toolbarTextCustomColor"
        implicitWidth: 32; implicitHeight: 32; Layout.maximumHeight: 32
        iconName: "plus"; label: "Cor personalizada do texto"; focusPolicy: Qt.NoFocus
        onClicked: customColor.choose(root.emphasis.color || root.values.color || root.canvas.penColor)
    }
    CustomColorDialog {
        id: customColor
        parent: root.Overlay.overlay
        onChosen: value => root.color(value)
        onClosed: if(root.editor.active)root.editor.focusText()
    }
}
