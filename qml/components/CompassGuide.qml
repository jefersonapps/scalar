import QtQuick
import QtQuick.Shapes
import "../theme"
Item {
    id: root
    property var canvas
    readonly property var geometry: canvas.compassGeometry
    readonly property real heading: Math.atan2(geometry.py-geometry.cy,geometry.px-geometry.cx)*180/Math.PI
    objectName: "compassGuide"
    visible: canvas.compassVisible
    anchors.fill: parent
    Repeater {
        model: [{x:root.geometry.cx,y:root.geometry.cy,pencil:false},
                {x:root.geometry.px,y:root.geometry.py,pencil:true}]
        Item {
            id: leg
            required property var modelData
            readonly property real legLength: Math.hypot(modelData.x-root.geometry.hx,modelData.y-root.geometry.hy)
            x: root.geometry.hx; y: root.geometry.hy
            width: legLength; height: Theme.compassLegWidth
            rotation: Math.atan2(modelData.y-root.geometry.hy,modelData.x-root.geometry.hx)*180/Math.PI
            transformOrigin: Item.TopLeft
            Rectangle {
                y: -height/2; width: leg.legLength*Theme.compassTipFraction; height: Theme.compassLegWidth
                radius: Theme.radiusSmall/2; border.width: Theme.hairline; border.color: Theme.compassMetalEdge
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.compassHighlight }
                    GradientStop { position: 0.45; color: Theme.compassMetal }
                    GradientStop { position: 1; color: Theme.compassMetalEdge }
                }
            }
            Rectangle {
                visible: leg.modelData.pencil
                x: leg.legLength*Theme.compassTipFraction-Theme.compassGripLength
                y: -height/2; width: Theme.compassGripLength; height: Theme.compassLegWidth+Theme.xs
                radius: Theme.xs; color: Theme.compassPencil; border.color: Theme.compassMetalEdge
            }
            Shape {
                ShapePath {
                    strokeWidth: Theme.hairline; strokeColor: Theme.compassMetalEdge
                    fillColor: leg.modelData.pencil ? Theme.compassGrip : Theme.compassHighlight
                    startX: leg.legLength*Theme.compassTipFraction; startY: -Theme.compassLegWidth/3
                    PathLine { x: leg.legLength; y: 0 }
                    PathLine { x: leg.legLength*Theme.compassTipFraction; y: Theme.compassLegWidth/3 }
                    PathLine { x: leg.legLength*Theme.compassTipFraction; y: -Theme.compassLegWidth/3 }
                }
            }
        }
    }
    Item {
        x: root.geometry.hx; y: root.geometry.hy
        rotation: root.heading; transformOrigin: Item.TopLeft
        Rectangle {
            x: -width/2; y: -Theme.compassJointSize/2-Theme.compassGripLength
            width: Theme.compassLegWidth; height: Theme.compassGripLength
            radius: Theme.xs; color: Theme.compassGrip; border.color: Theme.compassMetalEdge
        }
        Rectangle {
            x: -width/2; y: -height/2; width: Theme.compassJointSize; height: width; radius: width/2
            color: Theme.compassMetal; border.color: root.canvas.selectedGuide === "compass" ? Theme.guideInk : Theme.compassMetalEdge; border.width: Theme.focusBorder
            Rectangle { anchors.centerIn: parent; width: Theme.compassLegWidth; height: Theme.hairline; rotation: -45; color: Theme.compassGrip }
        }
    }
    Rectangle {
        x: root.geometry.ox-width/2; y: root.geometry.oy-height/2
        width: Theme.guideHandle; height: width; radius: Theme.xs
        rotation: root.heading; color: Theme.compassPencil; border.color: Theme.compassHighlight
        Icon { anchors.centerIn: parent; width: Theme.guideHandle-Theme.xs; height: width; name: "resize"; color: Theme.compassHighlight }
    }
    Rectangle {
        objectName: "compassDrawHandle"
        x: root.geometry.px-width/2; y: root.geometry.py-height/2
        width: Theme.xl; height: width; radius: width/2
        color: Theme.surface; border.color: Theme.compassPencil; border.width: Theme.focusBorder
        Icon { anchors.centerIn: parent; width: Theme.guideHandle; height: width; name: "rotate"; color: Theme.guideInk }
    }
    Text {
        x: root.geometry.hx+Theme.compassJointSize; y: root.geometry.hy-height-Theme.sm
        text: "r " + root.geometry.mm.toFixed(1) + " mm · Ø " + (root.geometry.mm*2).toFixed(1) + " mm"
        color: Theme.guideInk; font.pixelSize: Theme.caption
    }
}
