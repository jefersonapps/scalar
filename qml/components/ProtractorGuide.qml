import QtQuick
import "../theme"
Item {
    id: root
    property var canvas
    readonly property var geometry: canvas.protractorGeometry
    readonly property real radius: geometry.radius
    readonly property bool showNumbers: geometry.numbers
    readonly property real labelSize: Math.min(radius*.08, Math.max(11, radius*.035))
    readonly property real baseMargin: labelSize*1.8
    onShowNumbersChanged: face.requestPaint()
    objectName: "protractorGuide"
    visible: canvas.protractorVisible
    x: geometry.x-radius; y: geometry.y-radius
    width: radius*2; height: radius+baseMargin+1
    transform: Rotation { origin.x: root.radius; origin.y: root.radius; angle: root.geometry.angle }
    Canvas {
        id: face
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Connections { target: Theme; function onDarkChanged() { face.requestPaint() } }
        onPaint: {
            var ctx=getContext("2d"), r=root.radius
            ctx.reset(); ctx.clearRect(0,0,width,height)
            ctx.strokeStyle=Theme.guideInk; ctx.lineWidth=1
            ctx.beginPath(); ctx.arc(r,r,r-1,Math.PI,2*Math.PI)
            ctx.lineTo(2*r-1,r+root.baseMargin);ctx.lineTo(1,r+root.baseMargin);ctx.closePath()
            ctx.fillStyle=Theme.guideSurface; ctx.globalAlpha=.45; ctx.fill(); ctx.globalAlpha=1; ctx.stroke()
            function point(a,d) { return [r+Math.cos(a)*d,r-Math.sin(a)*d] }
            function line(a,inside,outside) {
                var p=point(a,inside),q=point(a,outside)
                ctx.beginPath();ctx.moveTo(p[0],p[1]);ctx.lineTo(q[0],q[1]);ctx.stroke()
            }
            var step=r<180?2:1, inner=r*.14, numbered=root.showNumbers
            var fontSize=root.labelSize, tick=Math.min(r*.16,Math.max(16,r*.075))
            var outerLabel=r-tick-fontSize, innerLabel=outerLabel-fontSize*1.5
            var spokeEnd=numbered?innerLabel-fontSize: r-tick
            ctx.font=fontSize+"px sans-serif"
            for(var degree=0;degree<=180;degree+=step) {
                var a=degree*Math.PI/180
                line(a,r-tick*(degree%10===0?1:degree%5===0?.65:.35),r-1)
                if(degree%10===0) {
                    ctx.globalAlpha=.45;line(a,inner,spokeEnd);ctx.globalAlpha=1
                    if(numbered && degree%(r<180?30:10)===0) {
                        ctx.save();var pos=point(a,degree===90?(outerLabel+innerLabel)/2:outerLabel);ctx.translate(pos[0],pos[1]);ctx.rotate(Math.PI/2-a)
                        ctx.fillStyle=Theme.guideInk;ctx.textAlign="center";ctx.textBaseline="middle"
                        if(degree===90) {
                            ctx.font=(fontSize*1.2)+"px sans-serif"
                            ctx.fillText("90",0,0)
                        } else {
                            ctx.fillText(String(degree),0,0)
                            ctx.globalAlpha=.85;ctx.fillText(String(180-degree),0,fontSize*1.5)
                        }
                        ctx.restore()
                    }
                }
            }
            if(numbered) {
                ctx.globalAlpha=.4
                var middleRadius=(outerLabel+innerLabel)/2
                var gap=Math.asin(Math.min(.9,fontSize*1.1/middleRadius))
                ctx.beginPath();ctx.arc(r,r,middleRadius,Math.PI,1.5*Math.PI-gap);ctx.stroke()
                ctx.beginPath();ctx.arc(r,r,middleRadius,1.5*Math.PI+gap,2*Math.PI);ctx.stroke()
                ctx.beginPath();ctx.arc(r,r,spokeEnd,Math.PI,2*Math.PI);ctx.stroke()
                ctx.globalAlpha=1
            }
            ctx.beginPath();ctx.arc(r,r,inner,Math.PI,2*Math.PI);ctx.stroke()
            ctx.beginPath();ctx.moveTo(r-9,r);ctx.lineTo(r+9,r);ctx.moveTo(r,r-9);ctx.lineTo(r,r+9);ctx.stroke()
        }
    }
    Rectangle {
        x: root.radius+root.radius*.72-width/2; y: root.radius-root.radius*.16-height/2
        width: 24; height: width; radius: width/2
        color: Theme.surface; border.color: Theme.guideInk
        Icon { anchors.centerIn: parent; width: 16; height: 16; name: "rotate"; color: Theme.guideInk }
    }
}
