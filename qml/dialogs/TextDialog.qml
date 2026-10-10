import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    // Qt 6.4's overlay wheel filter blocks sibling popups above a modal popup.
    // The font picker supplies the modal input guard while it is open.
    modal: !fonts.expanded
    dim: true
    objectName: "textDialog"
    property var draft: ({})
    signal applied(var values)
    function begin(values) { draft=Object.assign({},values);open() }
    function change(key,value) { const next=Object.assign({},draft);next[key]=value;draft=next }
    contentItem: ColumnLayout {
        spacing: Theme.md
        Text { text: "Formatar texto"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
        Text { text: "Fonte";color: Theme.secondary;font.pixelSize: Theme.caption }
        C.FontSelector {
            id: fonts
            objectName: "textFontSelector"
            Layout.fillWidth: true;fontFamily: root.draft.fontFamily || App.defaultTextFont
            onPicked: family => root.change("fontFamily",family)
        }
        Text { text: "Tamanho (pt)";color: Theme.secondary;font.pixelSize: Theme.caption }
        C.Field {
            Layout.fillWidth: true;text: root.draft.fontSizePt || 16
            validator: DoubleValidator { bottom: 6;top: 144;locale: "en_US" }
            onTextEdited: if(acceptableInput)root.change("fontSizePt",Number(text))
        }
        RowLayout {
            C.SelectField { Layout.fillWidth: true;model: ["Esquerda","Centro","Direita"];currentIndex: root.draft.alignment || 0;onActivated: root.change("alignment",index) }
        }
        Text { text: "Cor";color: Theme.secondary;font.pixelSize: Theme.caption }
        C.ColorPicker { Layout.fillWidth: true;selectedColor: root.draft.color || "#263345";onPicked: value => root.change("color",value.toString()) }
        C.ActionButton { text: "Cor personalizada";Layout.fillWidth: true;onClicked: customColor.choose(root.draft.color || "#263345") }
        Text { text: "LaTeX: $...$ no texto · $$...$$ em destaque.\nClique fora da caixa ou use Ctrl+Enter para concluir.";color: Theme.secondary;font.pixelSize: Theme.caption;Layout.fillWidth: true;wrapMode: Text.WordWrap }
        RowLayout {
            C.ActionButton { text: "Cancelar";Layout.fillWidth: true;onClicked: root.close() }
            C.ActionButton { objectName: "applyTextFormatButton";text: "Aplicar";primary: true;Layout.fillWidth: true;onClicked: { root.applied(root.draft);root.close() } }
        }
    }
    C.CustomColorDialog { id: customColor;objectName: "textCustomColorDialog";onChosen: value => root.change("color",value.toString()) }
}
