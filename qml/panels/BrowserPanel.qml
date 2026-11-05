import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Scalar 1.0
import "../components" as C
import "../theme"

FocusScope {
    id: root
    property bool suspended: false
    readonly property bool browserFocused: activeFocus || web.browserFocused
    property alias browser: web
    readonly property real cornerRadius: Math.max(0,Math.min(Theme.radiusFloating,width/2,height/2))
    signal closeRequested()
    Shape {
        anchors.fill: parent
        antialiasing: true
        ShapePath {
            strokeColor: Theme.border; strokeWidth: Theme.hairline; fillColor: Theme.surface
            startX: root.width-0.5; startY: 0.5
            PathLine { x: root.cornerRadius; y: 0.5 }
            PathQuad { x: 0.5; y: root.cornerRadius; controlX: 0.5; controlY: 0.5 }
            PathLine { x: 0.5; y: root.height-root.cornerRadius }
            PathQuad { x: root.cornerRadius; y: root.height-0.5; controlX: 0.5; controlY: root.height-0.5 }
            PathLine { x: root.width-0.5; y: root.height-0.5 }
            PathLine { x: root.width-0.5; y: 0.5 }
        }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: Theme.lg; spacing: Theme.sm
        RowLayout {
            Layout.fillWidth: true; spacing: Theme.sm
            C.Icon { name: "planet"; color: Theme.accent; Layout.preferredWidth: Theme.icon; Layout.preferredHeight: Theme.icon }
            Text { text: "Navegador"; color: Theme.text; font.pixelSize: Theme.body; font.weight: Font.DemiBold; Layout.fillWidth: true }
            C.IconButton { compact: true; iconName: "export"; label: "Abrir no navegador padrão"; onClicked: web.openExternal() }
            C.IconButton { objectName: "closeBrowserButton"; compact: true; iconName: "close"; label: "Fechar navegador"; onClicked: root.closeRequested() }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: Theme.xs
            C.IconButton { objectName: "browserBackButton"; compact: true; iconName: "back"; label: "Voltar"; enabled: web.canGoBack; onClicked: web.goBack() }
            C.IconButton { objectName: "browserForwardButton"; compact: true; iconName: "arrow"; label: "Avançar"; enabled: web.canGoForward; onClicked: web.goForward() }
            C.IconButton { objectName: "browserReloadButton"; compact: true; iconName: web.loading ? "close" : "rotate"; label: web.loading ? "Parar carregamento" : "Recarregar"; onClicked: web.loading ? web.stop() : web.reload() }
            C.Field {
                id: address; objectName: "browserAddressField"
                Layout.fillWidth: true; placeholderText: "Pesquisar ou digitar endereço"
                Accessible.name: "Endereço ou pesquisa na web"
                text: "https://www.google.com"
                selectByMouse: true
                onAccepted: { web.navigate(text); focus=false }
            }
        }
        Text { text: "Arraste imagens para o quadro · Arraste a divisória para redimensionar"; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.caption }
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            SystemBrowser {
                id: web; objectName: "systemBrowser"; anchors.fill: parent
                suspended: root.suspended || failure.visible
                onChanged: if(!address.activeFocus)address.text=url.toString()
                onFocusAddressRequested: { address.forceActiveFocus();address.selectAll() }
            }
            BusyIndicator { anchors.centerIn: parent; running: web.loading && !web.ready; visible: running }
            ColumnLayout {
                id: failure
                anchors.centerIn: parent; width: Math.min(320,Math.max(0,parent.width-Theme.xl))
                spacing: Theme.md
                visible: web.error.length>0
                Text { text: web.error; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; color: Theme.secondary; font.pixelSize: Theme.body }
                C.ActionButton { text: "Tentar novamente"; Layout.alignment: Qt.AlignHCenter; onClicked: web.reload() }
                C.ActionButton { text: "Abrir externamente"; Layout.alignment: Qt.AlignHCenter; iconName: "export"; onClicked: web.openExternal() }
            }
        }
    }
    Item {
        id: loadingTrack; objectName: "browserLoadingBar"
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        anchors.leftMargin: root.cornerRadius; anchors.rightMargin: Theme.hairline
        height: 3; clip: true
        visible: web.loading
        Accessible.role: Accessible.ProgressBar
        Accessible.name: "Carregando página"
        Accessible.ignored: !visible
        property real phase: 0
        Rectangle {
            id: loadingSegment
            width: App.reducedEffects ? parent.width : Math.max(60,parent.width*0.28)
            height: parent.height; radius: height/2; color: Theme.accent
            x: App.reducedEffects ? 0 : (loadingTrack.width-width)*loadingTrack.phase
        }
        SequentialAnimation {
            loops: Animation.Infinite
            running: loadingTrack.visible && !App.reducedEffects
            NumberAnimation { target: loadingTrack; property: "phase"; from: 0; to: 1; duration: 1000; easing.type: Easing.InOutSine }
            NumberAnimation { target: loadingTrack; property: "phase"; from: 1; to: 0; duration: 1000; easing.type: Easing.InOutSine }
        }
    }
    Connections { target: root.Window.window; function onActiveFocusItemChanged() { if(address.activeFocus)address.selectAll() } }
}
