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
    property bool finishAfterFormatting: false
    readonly property bool formattingOpen: formatting.visible
    property string objectId: ""
    property var draft: ({})
    property var defaults: ({fontFamily:App.defaultTextFont,fontSizePt:16,bold:false,italic:false,alignment:0,lineSpacing:1})
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
        canvas.tool="text"
        objectId=id
        draft=id ? App.textValues(id) : Object.assign({},defaults,{source:"",latexEnabled:App.sessionLatexEnabled,color:defaults.color || canvas.penColor.toString()})
        origin=id ? Qt.point(draft.x,draft.y) : Qt.point(box.x,box.y)
        boxWidth=id ? draft.width : box.width
        boxHeight=id ? draft.height : box.height
        angle=id ? draft.rotation : 0
        editor.text=draft.source
        canvas.editingTextId=id
        active=true
        Qt.callLater(function(){ App.restoreTextFormats(editor.textDocument,root.draft.formats || []);root.formatRevision++;App.setTextLineSpacing(editor.textDocument,root.draft.lineSpacing || 1);editor.forceActiveFocus();if(id)editor.selectAll() })
    }
    function focusText() { editor.forceActiveFocus() }
    function change(key,value) {
        const next=Object.assign({},draft);next[key]=value;draft=next
        if(key==="lineSpacing")App.setTextLineSpacing(editor.textDocument,value)
        if(key==="latexEnabled")App.sessionLatexEnabled=value
    }
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
    function finishForImport() {
        if(formatting.visible){finishAfterFormatting=true;formatting.applied(formatting.draft);formatting.close()}
        else finish()
    }
    function cancel() { active=false;canvas.editingTextId="";canvas.tool="select";canvas.forceActiveFocus();finished() }
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
        height: Math.max(root.boxHeight*root.pixelsPerMm,(editor.contentHeight+editor.topPadding+editor.bottomPadding)*canvas.zoom)
        rotation: root.angle; transformOrigin: Item.TopLeft
        color: "transparent"; border.color: Theme.accent; border.width: Theme.hairline
        TextArea {
            id: editor; objectName: "textSource"
            width: parent.width/root.canvas.zoom
            height: parent.height/root.canvas.zoom
            scale: root.canvas.zoom
            transformOrigin: Item.TopLeft
            enabled: !root.committing
            padding: 2
            color: root.draft.color || Theme.text
            property bool latexEnabled: root.draft.latexEnabled !== false
            property color syntaxBackground: App.pageColor
            placeholderText: "Escreva aqui…"
            placeholderTextColor: App.pageColor.hslLightness<0.5 ? "#a1a1aa" : "#66798b"
            font: App.textFont(root.draft.fontFamily || App.defaultTextFont,Math.max(1,Math.round(root.draft.fontSizePt*96/72)),root.draft.bold || false,root.draft.italic || false)
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
        Rectangle {
            objectName: "inlineTextBusyIndicator"
            visible: root.committing && App.textBusy
            x: 0; y: box.height+Theme.xs
            width: busyContent.width+2*Theme.sm; height: busyContent.height+2*Theme.xs
            radius: Theme.radiusSmall; color: Theme.surface; border.color: Theme.accent
            Accessible.role: Accessible.StaticText
            Accessible.name: busyLabel.text
            Row {
                id: busyContent
                anchors.centerIn: parent; spacing: Theme.xs
                BusyIndicator { width: 20; height: 20; running: parent.parent.visible }
                Text {
                    id: busyLabel
                    objectName: "inlineTextBusyLabel"
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.draft.latexEnabled !== false ? "Compilando LaTeX…" : "Salvando texto…"
                    color: Theme.text; font.pixelSize: Theme.caption
                }
            }
        }
        Repeater {
            model: [{left:true,top:true},{left:false,top:true},{left:true,top:false},{left:false,top:false}]
            Rectangle {
                required property var modelData
                objectName: modelData.left ? (modelData.top ? "inlineTextResizeTopLeft" : "inlineTextResizeBottomLeft") : (modelData.top ? "inlineTextResizeTopRight" : "inlineTextResizeHandle")
                x: (modelData.left ? 0 : box.width)-width/2
                y: (modelData.top ? 0 : box.height)-height/2
                width: Theme.handleSize; height: Theme.handleSize; radius: 3
                color: Theme.surface; border.color: Theme.accent; border.width: Theme.focusBorder
                MouseArea {
                    anchors.centerIn: parent; width: Theme.touch; height: Theme.touch
                    enabled: !root.committing; cursorShape: parent.modelData.left===parent.modelData.top ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                    preventStealing: true
                    property point startPointer
                    property point startOrigin
                    property real startWidth
                    property real startHeight
                    onPressed: mouse => {
                        startPointer=mapToItem(root,mouse.x,mouse.y);startOrigin=root.origin
                        startWidth=root.boxWidth;startHeight=root.boxHeight;mouse.accepted=true
                    }
                    onPositionChanged: mouse => {
                        if(!pressed)return
                        const p=mapToItem(root,mouse.x,mouse.y),a=root.angle*Math.PI/180
                        const dx=((p.x-startPointer.x)*Math.cos(a)+(p.y-startPointer.y)*Math.sin(a))/root.pixelsPerMm
                        const dy=(-(p.x-startPointer.x)*Math.sin(a)+(p.y-startPointer.y)*Math.cos(a))/root.pixelsPerMm
                        const left=parent.modelData.left,top=parent.modelData.top
                        root.boxWidth=Math.max(5,Math.min(10000,startWidth+(left ? -dx : dx)))
                        root.boxHeight=Math.max(5,Math.min(10000,startHeight+(top ? -dy : dy)))
                        const x=left ? startWidth-root.boxWidth : 0,y=top ? startHeight-root.boxHeight : 0
                        root.origin=Qt.point(startOrigin.x+x*Math.cos(a)-y*Math.sin(a),startOrigin.y+x*Math.sin(a)+y*Math.cos(a))
                    }
                    onReleased: editor.forceActiveFocus()
                }
            }
        }
    }
    TextDialog {
        id: formatting
        onApplied: values => {
            const changedColor=!Qt.colorEqual(values.color || Theme.text,root.dialogColor)
            if(values.latexEnabled!==root.draft.latexEnabled)App.sessionLatexEnabled=values.latexEnabled!==false
            root.draft=Object.assign({},values,{color:root.draft.color})
            if(changedColor)root.applyColor(values.color,root.formatStart,root.formatEnd)
            else { editor.forceActiveFocus();editor.select(root.formatStart,root.formatEnd) }
        }
        onClosed: {
            editor.forceActiveFocus()
            if(root.finishAfterFormatting){root.finishAfterFormatting=false;Qt.callLater(root.finish)}
        }
    }
    Text {
        visible: App.textError!=="";text: App.textError;color: Theme.text
        x: box.x; y: box.y+box.height+Theme.sm; width: box.width; wrapMode: Text.WordWrap
    }
}
