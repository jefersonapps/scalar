import QtQuick
import "../theme"
Rectangle {
    id: root
    color: Theme.surface
    radius: Theme.radiusFloating
    border.color: Theme.border
    border.width: 1
    antialiasing: true
    Canvas {
        id: shadow
        readonly property real margin: 16
        readonly property real panelRadius: root.radius
        readonly property color shadowColor: Theme.shadow
        x: -margin; y: -margin
        width: root.width+2*margin; height: root.height+2*margin
        visible: !App.reducedEffects
        z: -1
        contextType: "2d"
        smooth: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPanelRadiusChanged: requestPaint()
        onShadowColorChanged: requestPaint()
        onVisibleChanged: if(visible)requestPaint()
        onPaint: {
            const ctx=getContext("2d")
            ctx.reset()
            ctx.clearRect(0,0,width,height)
            const r=Math.min(panelRadius,root.width/2,root.height/2)
            ctx.beginPath()
            ctx.roundedRect(margin,margin,root.width,root.height,r,r)
            ctx.fillStyle="black"
            ctx.shadowColor=shadowColor.toString()
            ctx.shadowBlur=12
            ctx.fill()
            // Keep the panel interior transparent without separate outline rings.
            ctx.shadowColor="transparent"
            ctx.shadowBlur=0
            ctx.globalCompositeOperation="destination-out"
            ctx.fill()
        }
    }
}
