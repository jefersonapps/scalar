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
        export: "M12 15 V3 M7 8 L12 3 L17 8 M4 14 V19 Q4 21 6 21 H18 Q20 21 20 19 V14",
        palette: "M12 3 A9 9 0 1 0 12 21 H14 Q17 21 16 18 Q15 16 18 15 Q22 14 21 10 Q20 3 12 3 Z M7 9 H7.01 M10 6 H10.01 M15 6 H15.01 M18 10 H18.01",
        text: "M4 5 V3 H20 V5 M12 3 V21 M8 21 H16",
        background: "M4 4 H20 V20 H4 Z M9 4 V20 M15 4 V20 M4 9 H20 M4 15 H20",
        eraser: "M4 13 L13 4 Q15 2 17 4 L21 8 Q23 10 21 12 L12 21 H7 L3 17 Q2 15 4 13 Z M8 9 L16 17 M12 21 H22",
        image: "M5 3 H19 Q21 3 21 5 V19 Q21 21 19 21 H5 Q3 21 3 19 V5 Q3 3 5 3 Z M3 16 L8 11 Q9 10 10 11 L14 15 L16 13 Q17 12 18 13 L21 16 M17 7 A1 1 0 1 1 15 7 A1 1 0 1 1 17 7",
        select: "M5 3 Q4 2 4 4 V19 Q4 20 5 19 L9 15 L12 21 Q13 22 14 21 L15 20 Q16 19 15 18 L12 13 L19 12 Q21 12 19 11 Z",
        shapes: "M11 6 A4 4 0 1 1 3 6 A4 4 0 1 1 11 6 M15 10 H20 Q22 10 22 12 V19 Q22 21 20 21 H15 Q13 21 13 19 V12 Q13 10 15 10 Z M6 13 L11 21 H1 Z",
        trash: "M4 6 H20 M9 6 V4 Q9 3 10 3 H14 Q15 3 15 4 V6 M6 6 L7 19 Q7 21 9 21 H15 Q17 21 17 19 L18 6 M10 10 V17 M14 10 V17",
        duplicate: "M10 8 H19 Q21 8 21 10 V19 Q21 21 19 21 H10 Q8 21 8 19 V10 Q8 8 10 8 Z M5 16 Q3 16 3 14 V5 Q3 3 5 3 H14 Q16 3 16 5",
        pen: "M4 20 L5 15 L16 4 Q18 2 20 4 Q22 6 20 8 L9 19 Z M14 6 L18 10 M5 15 L9 19",
        hand: "M8 11 L8 4 Q8 2 10 2 Q12 2 12 4 L12 11 M12 7 Q12 5 14 5 Q16 5 16 7 L16 12 M16 8 Q16 6 18 6 Q20 6 20 8 L20 15 Q20 22 13 22 Q9 22 7 18 L3 12 Q2 10 4 9 Q5 9 8 12",
        undo: "M8 5 L3 10 L8 15 M3 10 L15 10 Q21 10 21 16 L21 19",
        redo: "M16 5 L21 10 L16 15 M21 10 L9 10 Q3 10 3 16 L3 19",
        plus: "M12 4 L12 20 M4 12 L20 12",
        minus: "M4 12 L20 12",
        back: "M14 5 L7 12 L14 19",
        folder: "M3 8 V6 Q3 4 5 4 H9 Q10 4 11 5 L13 7 H19 Q21 7 21 9 V18 Q21 20 19 20 H5 Q3 20 3 18 Z",
        save: "M5 3 H17 Q18 3 19 4 L21 6 V19 Q21 21 19 21 H5 Q3 21 3 19 V5 Q3 3 5 3 Z M7 3 V8 Q7 9 8 9 H16 Q17 9 17 8 V3 M7 21 V15 Q7 14 8 14 H16 Q17 14 17 15 V21",
        settings: "M5 3 V8 M5 12 V21 M12 3 V14 M12 18 V21 M19 3 V5 M19 9 V21 M7 10 A2 2 0 1 1 3 10 A2 2 0 1 1 7 10 M14 16 A2 2 0 1 1 10 16 A2 2 0 1 1 14 16 M21 7 A2 2 0 1 1 17 7 A2 2 0 1 1 21 7",
        fit: "M3 9 L3 3 L9 3 M15 3 L21 3 L21 9 M21 15 L21 21 L15 21 M9 21 L3 21 L3 15",
        close: "M5 5 L19 19 M19 5 L5 19",
        check: "M4 12 L9 17 L20 6",
        arrow: "M4 12 L20 12 M14 6 L20 12 L14 18",
        page: "M6 3 H14 L20 9 V19 Q20 21 18 21 H6 Q4 21 4 19 V5 Q4 3 6 3 Z M14 3 V7 Q14 9 16 9 H20 M8 13 H16 M8 17 H14",
        chevron: "M6 9 L12 15 L18 9",
        line: "M4 19 L20 5",
        circle: "M21 12 A9 9 0 1 1 3 12 A9 9 0 1 1 21 12",
        ellipse: "M22 12 A10 7 0 1 1 2 12 A10 7 0 1 1 22 12",
        rectangle: "M5 4 H19 Q21 4 21 6 V18 Q21 20 19 20 H5 Q3 20 3 18 V6 Q3 4 5 4 Z",
        triangle: "M11 4 Q12 2 13 4 L22 19 Q23 21 21 21 H3 Q1 21 2 19 Z",
        moon: "M20 15 A9 9 0 1 1 9 3 A8 8 0 0 0 20 15"
    })
    Shape {
        width: 24; height: 24
        anchors.centerIn: parent
        scale: Math.min(root.width, root.height) / 24
        ShapePath {
            strokeColor: root.color
            strokeWidth: Theme.iconStroke
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.paths[root.name] || root.paths.page }
        }
    }
}
