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
    property int formatStart: 0
    property int formatEnd: 0
    property color dialogColor: Theme.text
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
        draft=id ? App.textValues(id) : ({source:"",fontFamily:App.defaultTextFont,fontSizePt:16,bold:false,italic:false,alignment:0,color:canvas.penColor.toString()})
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
    function openFormatting() {
        formatStart=editor.selectionStart;formatEnd=editor.selectionEnd
        change("source",editor.text)
        const values=Object.assign({},draft)
        if(formatStart!==formatEnd && selectionStyle.color)values.color=selectionStyle.color
        dialogColor=values.color || Theme.text
        formatting.begin(values)
    }
    function applyColor(value,start,end) {
        if(start===undefined){start=editor.selectionStart;end=editor.selectionEnd}
        if(start===end){change("color",value.toString());App.colorTextSelection(editor.textDocument,0,editor.text.length,value)}
        else App.colorTextSelection(editor.textDocument,start,end,value)
        formatRevision++;editor.forceActiveFocus();editor.select(start,end)
    }
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
            property color syntaxBackground: App.pageColor
            placeholderText: "Escreva aqui…"
            placeholderTextColor: App.pageColor.hslLightness<0.5 ? "#a1a1aa" : "#66798b"
            font: App.textFont(root.draft.fontFamily || App.defaultTextFont,Math.max(1,root.draft.fontSizePt*25.4/72*root.pixelsPerMm),root.draft.bold || false,root.draft.italic || false)
            horizontalAlignment: root.draft.alignment===1 ? TextEdit.AlignHCenter : root.draft.alignment===2 ? TextEdit.AlignRight : TextEdit.AlignLeft
            wrapMode: TextEdit.Wrap; selectByMouse: true
            persistentSelection: true
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
        TextEditingToolbar {
            objectName: "inlineTextControls"
            x: 0; y: -height-Theme.xs
            bold: root.selectionStyle.bold || false; italic: root.selectionStyle.italic || false
            selectedColor: root.selectionStyle.color || root.draft.color || Theme.text
            onFormatRequested: root.openFormatting()
            onBoldRequested: root.toggleEmphasis(true)
            onItalicRequested: root.toggleEmphasis(false)
            onColorPicked: value => root.applyColor(value)
        }
        Rectangle {
            objectName: "inlineTextResizeHandle"
            x: box.width-width/2; y: box.height-height/2
            width: Theme.handleSize; height: Theme.handleSize; radius: 3
            color: Theme.surface; border.color: Theme.accent; border.width: Theme.focusBorder
            MouseArea {
                id: resizeArea
                anchors.centerIn: parent; width: Theme.touch; height: Theme.touch
                enabled: !root.committing; cursorShape: Qt.SizeFDiagCursor
                preventStealing: true
                onPressed: mouse => mouse.accepted=true
                onPositionChanged: mouse => {
                    if(!pressed)return
                    const point=mapToItem(box,mouse.x,mouse.y)
                    root.boxWidth=Math.max(5,Math.min(10000,point.x/root.pixelsPerMm))
                    root.boxHeight=Math.max(5,Math.min(10000,point.y/root.pixelsPerMm))
                }
                onReleased: editor.forceActiveFocus()
            }
        }
    }
    TextDialog {
        id: formatting
        onApplied: values => {
            const changedColor=!Qt.colorEqual(values.color || Theme.text,root.dialogColor)
            root.draft=Object.assign({},values,{color:root.draft.color})
            if(changedColor)root.applyColor(values.color,root.formatStart,root.formatEnd)
            else { editor.forceActiveFocus();editor.select(root.formatStart,root.formatEnd) }
        }
        onClosed: editor.forceActiveFocus()
    }
    Text {
        visible: App.textError!=="";text: App.textError;color: Theme.text
        x: box.x; y: box.y+box.height+Theme.sm; width: box.width; wrapMode: Text.WordWrap
    }
}
