import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme"
GlassPanel {
    id: root
    property var recorder
    signal exportRequested(string format)
    objectName: "constructionTimeline"
    visible: recorder.recording || recorder.pending || recorder.count>0 || recorder.error!==""
    height: column.implicitHeight+Theme.sm*2
    radius: Theme.radiusMedium
    ColumnLayout {
        id: column
        anchors.fill: parent; anchors.margins: Theme.sm
        spacing: Theme.xs
        RowLayout {
            Layout.fillWidth: true; spacing: Theme.xs
            Rectangle { Layout.leftMargin: Theme.sm; Layout.rightMargin: Theme.md; width: 8; height: 8; radius: 4; color: root.recorder.recording ? "#ef4444" : Theme.accent }
            Text { visible: root.width>=600; text: root.recorder.recording ? "Gravando passos" : root.recorder.pending ? "Preparando passos…" : "Construção"; color: Theme.text; font.pixelSize: Theme.caption }
            IconButton { implicitWidth: 32; implicitHeight: 32; iconName: "previousStep"; label: "Passo anterior"; enabled: !root.recorder.recording && !root.recorder.pending && root.recorder.position>0; onClicked: root.recorder.seek(root.recorder.position-1) }
            IconButton { objectName: "constructionPlayButton"; implicitWidth: 32; implicitHeight: 32; iconName: root.recorder.playing ? "pause" : "play"; label: root.recorder.playing ? "Pausar construção" : "Reproduzir construção"; enabled: !root.recorder.recording && !root.recorder.pending && root.recorder.count>1; onClicked: root.recorder.togglePlayback() }
            IconButton { implicitWidth: 32; implicitHeight: 32; iconName: "nextStep"; label: "Próximo passo"; enabled: !root.recorder.recording && !root.recorder.pending && root.recorder.position<root.recorder.count-1; onClicked: root.recorder.seek(root.recorder.position+1) }
            Slider {
                id: scrubber
                objectName: "constructionTimelineSlider"
                Layout.fillWidth: true; Layout.minimumWidth: 40; implicitHeight: 32
                from: 0; to: Math.max(1,root.recorder.count-1); stepSize: 1
                value: Math.max(0,root.recorder.position)
                enabled: !root.recorder.recording && !root.recorder.pending && root.recorder.count>0
                Accessible.name: "Passo da construção"
                onMoved: root.recorder.seek(Math.round(value))
                background: Rectangle {
                    x: scrubber.leftPadding+6; y: scrubber.topPadding+(scrubber.availableHeight-height)/2
                    width: Math.max(0,scrubber.availableWidth-12); height: 3; radius: 1.5; color: Theme.border
                    Rectangle { width: scrubber.visualPosition*parent.width; height: parent.height; radius: parent.radius; color: Theme.accent }
                }
                handle: Item {
                    objectName: "constructionPlayhead"
                    x: scrubber.leftPadding+scrubber.visualPosition*(scrubber.availableWidth-width)
                    y: scrubber.topPadding+(scrubber.availableHeight-height)/2
                    width: 12; height: 26
                    Rectangle { x: 5; y: 7; width: 2; height: 19; color: Theme.accent }
                    Canvas {
                        width: 12; height: 10
                        onPaint: {const c=getContext("2d");c.reset();c.beginPath();c.moveTo(0,0);c.lineTo(12,0);c.lineTo(12,5);c.lineTo(6,10);c.lineTo(0,5);c.closePath();c.fillStyle=Theme.accent;c.fill()}
                    }
                    MouseArea {
                        anchors.fill: parent; anchors.margins: -5
                        acceptedButtons: Qt.RightButton
                        onPressed: mouse => {
                            if(root.recorder.position<0)root.recorder.seek(Math.round(scrubber.value))
                            const p=mapToItem(root,mouse.x,mouse.y);stepMenu.x=Math.max(0,Math.min(p.x,root.width-stepMenu.width));stepMenu.y=-stepMenu.implicitHeight-Theme.sm;stepMenu.open()
                        }
                    }
                }
            }
            Text { text: root.recorder.position<0 ? Math.max(0,root.recorder.count-1)+" passos" : root.recorder.position+" / "+Math.max(0,root.recorder.count-1); color: Theme.secondary; font.pixelSize: Theme.caption }

            ActionButton { implicitHeight: 32; text: root.width>=600 ? "Exportar" : ""; tooltip: "Exportar animação"; iconName: "export"; enabled: !root.recorder.recording && !root.recorder.pending && !root.recorder.exporting && root.recorder.count>1; onClicked: exports.open() }
            IconButton { implicitWidth: 32; implicitHeight: 32; iconName: "close"; label: "Descartar gravação"; enabled: !root.recorder.recording && !root.recorder.pending && !root.recorder.exporting; onClicked: root.recorder.clear() }
        }
        Text { visible: root.recorder.error!=="" || root.recorder.exporting; text: root.recorder.error || "Exportando animação…"; color: root.recorder.error ? Theme.danger : Theme.secondary; font.pixelSize: Theme.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
    GlassPopover {
        id: stepMenu
        objectName: "constructionStepMenu"
        width: 200; padding: Theme.sm
        contentItem: MenuAction {
            objectName: "deleteConstructionStep"
            text: "Apagar passo"; iconName: "deleteStep"
            enabled: !root.recorder.recording && !root.recorder.pending && !root.recorder.exporting && root.recorder.position>=0
            onClicked: {stepMenu.close();root.recorder.deleteStep(root.recorder.position)}
        }
    }
    GlassPopover {
        id: exports
        y: -height-Theme.sm; x: Math.max(0,root.width-width); width: 220; padding: Theme.sm
        contentItem: ColumnLayout {
            spacing: Theme.xs
            MenuAction { text: "Vídeo MP4…"; iconName: "export"; Layout.fillWidth: true; onClicked: {exports.close();root.exportRequested("mp4")} }
            MenuAction { text: "Animação GIF…"; iconName: "export"; Layout.fillWidth: true; onClicked: {exports.close();root.exportRequested("gif")} }
            Text { visible: !root.recorder.encoderAvailable; text: "A exportação requer FFmpeg instalado."; color: Theme.secondary; font.pixelSize: Theme.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
    }
}
