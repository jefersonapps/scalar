import QtQuick
import "../theme"
Item {
    id: root
    property var canvas
    readonly property var geometry: canvas.rulerGeometry
    readonly property real tickStep: Math.max(1,Math.ceil(2/geometry.scale))
    readonly property real labelStep: 10*Math.max(1,Math.ceil(26/(10*geometry.scale)))
    objectName: "rulerGuide"
    visible: canvas.rulerVisible
    x: geometry.x; y: geometry.y
    width: geometry.length; height: geometry.width
    rotation: geometry.angle; transformOrigin: Item.TopLeft
    Rectangle { anchors.fill: parent; radius: Theme.xs; color: Theme.guideSurface; border.color: Theme.guideInk; border.width: root.canvas.selectedGuide === "ruler" ? Theme.focusBorder : Theme.hairline }
    Text { x: (root.width-width)/2; y: -height-Theme.sm; text: root.geometry.mm.toFixed(0) + " mm · " + root.geometry.angle.toFixed(1) + "°"; color: Theme.guideInk; font.pixelSize: Theme.caption }
    Text { x: root.width-width-Theme.md; y: root.height-height-Theme.xs; text: "cm"; color: Theme.guideInk; font.pixelSize: Theme.caption }
    Repeater {
        model: Math.floor(root.geometry.mm/root.tickStep)+1
        Rectangle {
            required property int index
            readonly property real mm: index*root.tickStep
            x: mm*root.geometry.scale; width: Theme.hairline; height: (mm%10===0?4:2)*root.geometry.scale
            color: Theme.guideInk
        }
    }
    Repeater {
        model: Math.floor(root.geometry.mm/root.labelStep)+1
        Text {
            required property int index
            visible: index*root.labelStep*root.geometry.scale < root.width-Theme.xxl-Theme.md
            x: index*root.labelStep*root.geometry.scale-width/2; y: root.height-height-Theme.xs
            text: (index*root.labelStep/10).toFixed(0); color: Theme.guideInk; font.pixelSize: Theme.caption
        }
    }
    Repeater {
        model: [{x:0,icon:"rotate"},{x:root.width/2,icon:"hand"},{x:root.width,icon:"plus"}]
        Rectangle {
            required property var modelData
            x: modelData.x-width/2; y: root.height/2-height/2; width: Theme.guideHandle; height: width; radius: width/2
            color: Theme.surface; border.color: Theme.guideInk; border.width: Theme.hairline
            Icon { anchors.centerIn: parent; width: Theme.guideHandle-Theme.xs; height: width; name: modelData.icon; color: Theme.guideInk }
        }
    }
}
