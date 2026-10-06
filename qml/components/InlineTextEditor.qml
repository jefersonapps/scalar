import QtQuick
import QtQuick.Controls
import "../theme"
import "../dialogs"
Item {
    id: root
    property var canvas
    objectName: "inlineTextEditor"
    signal finished()
    property bool active: false
    property bool committing: false
    property string objectId: ""
    property var draft: ({})
    property point origin: Qt.point(0,0)
    property real boxWidth: 60
    property real boxHeight: 18
    property real angle: 0
    property int formatRevision: 0
    readonly property var selectionStyle: {
        const revision=formatRevision;const text=editor.text;const cursor=editor.cursorPosition
        return App.textEmphasis(editor.textDocument,editor.selectionStart,editor.selectionEnd)
    }
    readonly property real pixelsPerMm: canvas ? canvas.zoom*96/25.4 : 96/25.4
    readonly property point screenOrigin: { const revision=canvas ? canvas.zoom : 1; return canvas ? canvas.screenPoint(origin) : origin }
    visible: active
    function begin(box,id) {
        if(active)return
        objectId=id
        draft=id ? App.textValues(id) : ({source:"",fontFamily:App.defaultTextFont,fontSizePt:18,bold:false,italic:false,alignment:0,color:canvas.penColor.toString()})
        origin=id ? Qt.point(draft.x,draft.y) : Qt.point(box.x,box.y)
        boxWidth=id ? draft.width : box.width
        boxHeight=id ? draft.height : box.height
        angle=id ? draft.rotation : 0
        editor.text=draft.source
        canvas.editingTextId=id
        active=true
        Qt.callLater(function(){ App.restoreTextFormats(editor.textDocument,root.draft.formats || []);root.formatRevision++;editor.forceActiveFocus();if(id)editor.selectAll() })
    }
    function change(key,value) { const next=Object.assign({},draft);next[key]=value;draft=next }
    function openFormatting() { change("source",editor.text);formatting.begin(draft) }
    function toggleEmphasis(bold) {
        const start=editor.selectionStart,end=editor.selectionEnd
        const enabled=!(bold ? selectionStyle.bold : selectionStyle.italic)
        if(start===end)change(bold ? "bold" : "italic",enabled)
        else App.formatTextSelection(editor.textDocument,start,end,bold,enabled)
        formatRevision++;editor.forceActiveFocus();editor.select(start,end)
    }
    function finish() {
        if(!active||committing||formatting.visible)return
        if(editor.text.trim()==="") { cancel();return }
        if(App.textBusy)return
        change("source",editor.text)
        change("formats",App.textFormats(editor.textDocument,draft.bold || false,draft.italic || false))
        change("boxWidthMm",boxWidth)
        change("boxHeightMm",boxHeight)
        committing=true
        App.upsertText(objectId,origin,draft)
        if(!App.textBusy)committing=false
    }
    function cancel() { active=false;canvas.editingTextId="";canvas.forceActiveFocus();finished() }
    Connections {
        target: App
        function onTextCommitted(id) {
            if(!root.committing)return
            root.committing=false;root.active=false;root.canvas.editingTextId=""
            root.canvas.tool="select";root.canvas.selectText(id);root.canvas.forceActiveFocus()
            root.finished()
        }
        function onChanged() { if(root.committing&&!App.textBusy&&App.textError!="")root.committing=false }
    }
    MouseArea { anchors.fill: parent; onClicked: root.finish() }
    Rectangle {
        id: box
        objectName: "inlineTextBox"
        x: root.screenOrigin.x; y: root.screenOrigin.y
        width: Math.max(24,root.boxWidth*root.pixelsPerMm)
        height: Math.max(root.boxHeight*root.pixelsPerMm,editor.contentHeight+editor.topPadding+editor.bottomPadding)
        rotation: root.angle; transformOrigin: Item.TopLeft
        color: "transparent"; border.color: Theme.accent; border.width: Theme.hairline
        TextArea {
            id: editor; objectName: "textSource"
            anchors.fill: parent
            enabled: !root.committing
            padding: 2*root.pixelsPerMm/(96/25.4)
            color: root.draft.color || Theme.text
            placeholderText: "Escreva aqui…"
            placeholderTextColor: App.pageColor.hslLightness<0.5 ? "#a1a1aa" : "#66798b"
            font: App.textFont(root.draft.fontFamily || App.defaultTextFont,Math.max(1,root.draft.fontSizePt*25.4/72*root.pixelsPerMm),root.draft.bold || false,root.draft.italic || false)
            horizontalAlignment: root.draft.alignment===1 ? TextEdit.AlignHCenter : root.draft.alignment===2 ? TextEdit.AlignRight : TextEdit.AlignLeft
            wrapMode: TextEdit.Wrap; selectByMouse: true
            textFormat: TextEdit.PlainText
            background: null
            Keys.onEscapePressed: root.cancel()
            Keys.onPressed: event => {
                if(event.modifiers&Qt.ControlModifier){
                    if(event.key===Qt.Key_Return){root.finish();event.accepted=true}
                    else if(event.key===Qt.Key_B||event.key===Qt.Key_I){root.toggleEmphasis(event.key===Qt.Key_B);event.accepted=true}
                }
            }
        }
        Row {
            x: 0; y: box.y<height+Theme.xs ? 0 : -height-Theme.xs
            spacing: Theme.xs
            IconButton {
                objectName: "textFormatButton"
                iconName: "settings";label: "Fonte, tamanho e cor";selected: true
                onClicked: root.openFormatting()
            }
            ActionButton {
                objectName: "inlineBoldButton";text: "B";implicitWidth: Theme.touch
                font.bold: true;checked: root.selectionStyle.bold || false;focusPolicy: Qt.NoFocus
                contentItem: Text { text: "B";font.bold: true;font.pixelSize: Theme.body;color: Theme.text;horizontalAlignment: Text.AlignHCenter;verticalAlignment: Text.AlignVCenter }
                Accessible.name: "Negrito no texto selecionado"
                onClicked: root.toggleEmphasis(true)
                ToolTip.visible: hovered;ToolTip.text: "Negrito (Ctrl+B)"
            }
            ActionButton {
                objectName: "inlineItalicButton";text: "I";implicitWidth: Theme.touch
                font.italic: true;checked: root.selectionStyle.italic || false;focusPolicy: Qt.NoFocus
                contentItem: Text { text: "I";font.italic: true;font.pixelSize: Theme.body;color: Theme.text;horizontalAlignment: Text.AlignHCenter;verticalAlignment: Text.AlignVCenter }
                Accessible.name: "Itálico no texto selecionado"
                onClicked: root.toggleEmphasis(false)
                ToolTip.visible: hovered;ToolTip.text: "Itálico (Ctrl+I)"
            }
        }
    }
    TextDialog {
        id: formatting
        onApplied: values => { root.draft=values;editor.forceActiveFocus() }
        onClosed: editor.forceActiveFocus()
    }
    Text {
        visible: App.textError!=="";text: App.textError;color: Theme.text
        x: box.x; y: box.y+box.height+Theme.sm; width: box.width; wrapMode: Text.WordWrap
    }
}
