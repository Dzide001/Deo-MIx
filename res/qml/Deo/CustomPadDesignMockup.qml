import QtQuick 2.12
import "../Library"
import "../Mixxx/Controls"

Item {
    width: 139
    height: 108

    Rectangle {
        id: rectangle
        x: 85
        y: 32
        width: 50
        height: 32
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle1
        x: 31
        y: 32
        width: 50
        height: 32
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle5
        x: 31
        y: 68
        width: 50
        height: 32
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle6
        x: 85
        y: 68
        width: 50
        height: 32
        color: "#0f2e3f"
    }


    Rectangle {
        id: rectangle8
        x: 32
        y: 15
        width: 103
        height: 5
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle2
        x: 1
        y: 4
        width: 18
        height: 100
        color: "#0f2e3f"
    }

    Text {
        id: text1
        x: -17
        y: 47
        color: "#91b1b4"
        text: "CUSTOM"
        font.pixelSize: 12
        rotation: 270
        font.bold: true
    }


    Rectangle {
        id: rectangle7
        x: 37
        y: 9
        width: 5
        height: 17
        color: "#427490"
        radius: 5
    }
}
