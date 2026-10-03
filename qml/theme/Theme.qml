pragma Singleton
import QtQuick
QtObject {
    readonly property bool dark: App.theme === "Dark" || (App.theme === "System" && App.systemDark)
    readonly property color background: dark ? "#09090b" : "#f3f5f7"
    readonly property color workspace: dark ? "#111113" : "#e9eef1"
    readonly property color surface: dark ? "#18181b" : "#ffffff"
    readonly property color glass: dark ? "#ed18181b" : "#edffffff"
    readonly property color hover: dark ? "#27272a" : "#edf3f5"
    readonly property color pressed: dark ? "#3f3f46" : "#dce9ec"
    readonly property color accent: dark ? "#fafafa" : "#167b69"
    readonly property color accentSoft: dark ? "#27272a" : "#dcefe9"
    readonly property color accentText: dark ? "#09090b" : "#ffffff"
    readonly property color text: dark ? "#fafafa" : "#243345"
    readonly property color secondary: dark ? "#a1a1aa" : "#66798b"
    readonly property color border: dark ? "#303033" : "#dce5e9"
    readonly property color shadow: dark ? "#70000000" : "#1026384a"
    readonly property color scrim: dark ? "#80000000" : "#60202020"
    readonly property var backgroundColors: ["#ffffff","#fff7e6","#fef3c7","#dcfce7","#dbeafe","#fce7f3","#ede9fe","#214f43","#18221e","#000000"]
    readonly property var basicColors: ["#ffffff","#000000","#ef4444","#f97316","#eab308","#22c55e","#14b8a6","#3b82f6","#8b5cf6","#ec4899"]
    readonly property var inkHues: [0.46,0.60,0.97,0.11,0.76]
    readonly property var inkSaturations: [0.65,0.75,0.65,0.75,0.65]
    readonly property var inkLightnessOnLight: [0.32,0.40,0.39,0.32,0.40]
    readonly property var inkLightnessOnDark: [0.72,0.75,0.76,0.72,0.78]
    readonly property color inkNeutralOnLight: "#18181b"
    readonly property color inkNeutralOnDark: "#f4f4f5"
    readonly property color danger: dark ? "#ffb4ab" : "#b33b3b"
    readonly property int xs: 4
    readonly property int sm: 8
    readonly property int md: 12
    readonly property int lg: 16
    readonly property int xl: 24
    readonly property int xxl: 32
    readonly property int section: 48
    readonly property int radiusSmall: 8
    readonly property int radiusMedium: 12
    readonly property int radiusLarge: 18
    readonly property int radiusFloating: 24
    readonly property int touch: 44
    readonly property int controlHeight: 40
    readonly property int sliderHandle: 18
    readonly property int toggleWidth: 32
    readonly property int toggleHeight: 18
    readonly property int settingsSegmentWidth: 248
    readonly property real iconStroke: 1.6
    readonly property int icon: 22
    readonly property int title: 36
    readonly property int heading: 20
    readonly property int body: 14
    readonly property int caption: 13
    readonly property int fast: App.reducedEffects ? 0 : 100
    readonly property int normal: App.reducedEffects ? 0 : 160
    readonly property int windowWidth: 1200
    readonly property int windowHeight: 800
    readonly property int minimumWidth: 520
    readonly property int minimumHeight: 640
    readonly property int compactBreakpoint: 600
    readonly property int mediumBreakpoint: 700
    readonly property int zoomBreakpoint: 650
    readonly property int hintBreakpoint: 850
    readonly property int tooltipDelay: 700
    readonly property int hairline: 1
    readonly property int focusBorder: 2
    readonly property real selectedIconScale: 1.06
    readonly property real pressedIconScale: 0.96
    readonly property int colorPlaneHeight: 160
    readonly property int colorHueHeight: 24
    readonly property int dialogWidth: 460
    readonly property int maxContent: 1040
    readonly property int topbarHeight: 72
    readonly property int navigationInlineBreakpoint: 1100
    readonly property int toolbarHeight: 56
    readonly property int handleSize: 12
    readonly property int propertiesWidth: 280
    readonly property int pagesPanelWidth: 344
    readonly property int pageTileHeight: 172
    readonly property int cardWidth: 280
    readonly property int cardHeight: 232
}
