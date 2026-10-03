import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as C
import "../theme"
C.ModernDialog {
    id: root
    objectName: "textDialog"
    property string objectId: ""
    property point position: Qt.point(20,20)
    property var draft: ({})
    function begin(point, id, color) {
        position = point; objectId = id
        draft = id.length ? App.textValues(id) : ({source: "", fontFamily: "", fontSizePt: 18, bold: false, italic: false, alignment: 0, color: color})
        editor.text = draft.source; open(); editor.forceActiveFocus()
    }
    function change(key, value) { const next = Object.assign({}, draft); next[key] = value; draft = next }
    Connections { target: App; function onTextCommitted(id) { if(root.visible) root.close() } }
    closePolicy: App.textBusy ? Popup.NoAutoClose : Popup.CloseOnEscape
    contentItem: ColumnLayout {
        spacing: Theme.md
        Text { text: root.objectId.length ? "Editar texto" : "Texto e matemática"; color: Theme.text; font.pixelSize: Theme.heading; font.weight: Font.DemiBold }
        ScrollView {
            Layout.fillWidth: true; Layout.preferredHeight: Theme.section*3; clip: true
            TextArea {
                id: editor; objectName: "textSource"
                enabled: !App.textBusy; color: Theme.text; placeholderTextColor: Theme.secondary
                placeholderText: "Escreva aqui ou use $x^2$ e $$\\frac{a}{b}$$"
                wrapMode: TextEdit.Wrap; selectByMouse: true; font.pixelSize: Theme.body
                background: Rectangle { color: Theme.background; radius: Theme.radiusSmall; border.color: Theme.border }
            }
        }
        RowLayout {
            C.Field { Layout.fillWidth: true; text: root.draft.fontFamily || ""; placeholderText: "Fonte do sistema"; onTextEdited: root.change("fontFamily",text) }
            C.Field { Layout.preferredWidth: Theme.section*2; text: root.draft.fontSizePt || 18; validator: DoubleValidator { bottom: 6; top: 144; locale: "en_US" } onTextEdited: if(acceptableInput) root.change("fontSizePt",Number(text)) }
        }
        RowLayout {
            C.ActionButton { text: "B"; checked: root.draft.bold || false; checkable: true; onClicked: root.change("bold",checked) }
            C.ActionButton { text: "I"; checked: root.draft.italic || false; checkable: true; onClicked: root.change("italic",checked) }
            C.SelectField { Layout.fillWidth: true; model: ["Esquerda", "Centro", "Direita"]; currentIndex: root.draft.alignment || 0; onActivated: root.change("alignment",index) }
        }
        C.ColorPicker { selectedColor: root.draft.color || "#263345"; onPicked: value => root.change("color",value) }
        Text { text: "LaTeX offline · $...$ no texto · $$...$$ em destaque"; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        Text { visible: App.textError.length > 0; text: App.textError; color: Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        RowLayout {
            C.ActionButton { text: "Cancelar"; enabled: !App.textBusy; Layout.fillWidth: true; onClicked: root.close() }
            C.ActionButton { text: App.textBusy ? "Preparando…" : "Inserir / salvar"; primary: true; enabled: !App.textBusy && editor.text.trim().length > 0; Layout.fillWidth: true; onClicked: { root.change("source",editor.text); App.upsertText(root.objectId,root.position,root.draft) } }
        }
    }
}
