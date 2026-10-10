import QtQuick
import "../theme"

Item {
    id: root
    property var canvas
    readonly property var guide: canvas ? canvas.segmentGuide : ({})
    visible: guide.angle !== undefined
    Canvas {
        id: drawing
        x: (root.guide.x || 0) - 56; y: (root.guide.y || 0) - 56
        width: 112; height: 112
        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            if (!root.visible) return
            const angle = root.guide.angle * Math.PI / 180
            ctx.lineWidth = 1; ctx.strokeStyle = Theme.secondary; ctx.globalAlpha = .3
            ctx.beginPath(); ctx.arc(56, 56, 32, 0, Math.PI * 2); ctx.stroke()
            ctx.globalAlpha = .7; ctx.beginPath()
            for (let x = 56; x < 104; x += 6) { ctx.moveTo(x, 56); ctx.lineTo(x + 3, 56) }
            ctx.stroke()
            ctx.globalAlpha = 1; ctx.strokeStyle = Theme.accent; ctx.lineWidth = 2
            ctx.beginPath(); ctx.arc(56, 56, 32, 0, -angle, true); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(56, 56); ctx.lineTo(56 + 48 * Math.cos(angle), 56 - 48 * Math.sin(angle)); ctx.stroke()
        }
    }
    AngleLabel {
        x: Math.max(8, Math.min(root.width - width - 8, (root.guide.x || 0) + 46))
        y: Math.max(8, Math.min(root.height - height - 8, (root.guide.y || 0) - 22))
        angle: root.guide.angle === undefined ? 0 : root.guide.angle
    }
    Connections { target: root.canvas; function onSelectionChanged() { drawing.requestPaint() } function onViewChanged() { drawing.requestPaint() } }
    Connections { target: Theme; function onDarkChanged() { drawing.requestPaint() } }
}
