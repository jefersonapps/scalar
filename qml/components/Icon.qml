import QtQuick
import QtQuick.Shapes
import "../theme"
Item {
    id: root
    property string name: "pen"
    property color color: Theme.text
    implicitWidth: Theme.icon
    implicitHeight: Theme.icon
    readonly property var paths: ({
        pen: "M4 20 L5 14 L16 3 Q17 2 19 4 Q21 6 20 7 L9 18 Z M14 5 L18 9 M5 14 L9 18",
        hand: "M8 11 L8 4 Q8 2 10 2 Q12 2 12 4 L12 11 M12 7 Q12 5 14 5 Q16 5 16 7 L16 12 M16 8 Q16 6 18 6 Q20 6 20 8 L20 15 Q20 22 13 22 Q9 22 7 18 L3 12 Q2 10 4 9 Q5 9 8 12",
        undo: "M8 5 L3 10 L8 15 M3 10 L15 10 Q21 10 21 16 L21 19",
        redo: "M16 5 L21 10 L16 15 M21 10 L9 10 Q3 10 3 16 L3 19",
        plus: "M12 4 L12 20 M4 12 L20 12",
        minus: "M4 12 L20 12",
        back: "M14 5 L7 12 L14 19",
        folder: "M3 7 L3 4 L10 4 L13 7 L21 7 L21 20 L3 20 Z",
        save: "M4 3 L18 3 L21 6 L21 21 L3 21 L3 3 Z M7 3 L7 9 L17 9 L17 3 M7 21 L7 14 L17 14 L17 21",
        settings: "M9 3 L15 3 L16 7 L20 9 L20 15 L16 17 L15 21 L9 21 L8 17 L4 15 L4 9 L8 7 Z M15 12 A3 3 0 1 1 9 12 A3 3 0 1 1 15 12",
        fit: "M3 9 L3 3 L9 3 M15 3 L21 3 L21 9 M21 15 L21 21 L15 21 M9 21 L3 21 L3 15",
        close: "M5 5 L19 19 M19 5 L5 19",
        check: "M4 12 L9 17 L20 6",
        arrow: "M4 12 L20 12 M14 6 L20 12 L14 18",
        page: "M6 2 L15 2 L20 7 L20 22 L4 22 L4 2 Z M15 2 L15 7 L20 7 M8 12 L16 12 M8 16 L16 16",
        chevron: "M6 9 L12 15 L18 9",
        moon: "M20 15 A9 9 0 1 1 9 3 A8 8 0 0 0 20 15"
    })
    Shape {
        width: 24; height: 24
        anchors.centerIn: parent
        scale: Math.min(root.width, root.height) / 24
        ShapePath {
            strokeColor: root.color
            strokeWidth: 1.7
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.paths[root.name] || root.paths.page }
        }
    }
}
