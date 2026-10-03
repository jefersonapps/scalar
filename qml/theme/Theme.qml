pragma Singleton
import QtQuick
QtObject {
    readonly property bool dark: App.theme === "Dark" || (App.theme === "System" && App.systemDark)
    readonly property color background: dark ? "#121b22" : "#f3f5f7"
    readonly property color workspace: dark ? "#19232b" : "#e9eef1"
    readonly property color surface: dark ? "#24313c" : "#ffffff"
    readonly property color glass: dark ? "#ed24313c" : "#edffffff"
    readonly property color hover: dark ? "#344451" : "#edf3f5"
    readonly property color pressed: dark ? "#3c505c" : "#dce9ec"
    readonly property color accent: dark ? "#69d4bf" : "#167b69"
    readonly property color accentSoft: dark ? "#28483f" : "#dcefe9"
    readonly property color accentText: dark ? "#122c27" : "#ffffff"
    readonly property color text: dark ? "#eef5f6" : "#243345"
    readonly property color secondary: dark ? "#aabdc8" : "#66798b"
    readonly property color border: dark ? "#425260" : "#dce5e9"
    readonly property color shadow: dark ? "#30101820" : "#1026384a"
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
    readonly property int touch: 48
    readonly property int icon: 22
    readonly property int title: 36
    readonly property int heading: 22
    readonly property int body: 15
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
    readonly property int dialogWidth: 460
    readonly property int maxContent: 1040
    readonly property int topbarHeight: 76
    readonly property int toolbarHeight: 64
    readonly property int cardWidth: 280
    readonly property int cardHeight: 232
}
