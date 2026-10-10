import QtQuick
import QtQuick.Effects

Item {
    id: root
    property Item toolbar: null
    ShaderEffectSource {
        id: backdrop
        width: root.toolbar ? root.toolbar.width : 0
        height: root.toolbar ? root.toolbar.height : 0
        visible: false
        sourceItem: root.toolbar ? root.toolbar.backdropSource : null
        sourceRect: {
            if (!root.toolbar || !sourceItem) return Qt.rect(0, 0, 0, 0)
            // Track layout changes to keep the crop aligned with the toolbar.
            const layoutX = root.toolbar.x, layoutY = root.toolbar.y
            const sourceWidth = sourceItem.width, sourceHeight = sourceItem.height
            const origin = root.toolbar.mapToItem(sourceItem, 0, 0)
            return Qt.rect(origin.x, origin.y, width, height)
        }
        textureSize: Qt.size(Math.max(1, Math.ceil(width / 4)), Math.max(1, Math.ceil(height / 4)))
        live: true
        hideSource: false
    }
    Rectangle {
        id: glassMask
        anchors.fill: parent
        radius: root.toolbar ? root.toolbar.radius : 0
        color: "white"
        visible: false
        layer.enabled: true
        layer.textureSize: Qt.size(Math.max(1, Math.ceil(width / 4)), Math.max(1, Math.ceil(height / 4)))
    }
    MultiEffect {
        // Run every blur pass at quarter resolution, then upscale once.
        width: root.width / 4
        height: root.height / 4
        scale: 4
        transformOrigin: Item.TopLeft
        source: backdrop
        blurEnabled: true
        blurMax: 8
        blur: 1
        autoPaddingEnabled: false
        maskEnabled: true
        maskSource: glassMask
    }
}
