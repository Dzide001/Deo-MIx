import QtQuick 2.12

Item {
    width: 252
    height: 108


    Rectangle {
        id: rectangle2
        x: 5
        y: 3
        width: 18
        height: 99
        color: "#0f2e3f"
    }


    Rectangle {
        id: rectangle3
        x: 28
        y: 69
        width: 52
        height: 33
        color: "#0f2e3f"
    }
    Text {
        id: text1
        x: -5
        y: 43
        color: "#91b1b4"
        text: "PADS"
        font.pixelSize: 12
        rotation: 270
        font.bold: true
    }

    Rectangle {
        id: rectangle5
        x: 28
        y: 3
        width: 107
        height: 25
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle6
        x: 83
        y: 69
        width: 52
        height: 33
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle7
        x: 138
        y: 69
        width: 52
        height: 33
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle8
        x: 193
        y: 69
        width: 52
        height: 33
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle4
        x: 28
        y: 33
        width: 52
        height: 33
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle9
        x: 83
        y: 33
        width: 52
        height: 33
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle10
        x: 138
        y: 33
        width: 52
        height: 33
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle11
        x: 193
        y: 33
        width: 52
        height: 33
        color: "#0f2e3f"
    }

    Text {
        id: text3
        x: 38
        y: 8
        color: "#91b1b4"
        text: "STEMS"
        font.pixelSize: 12
        rotation: 0
        font.bold: true
    }

    Text {
        id: text4
        x: 123
        y: 10
        color: "#91b1b4"
        text: "V"
        font.pixelSize: 10
        rotation: 0
        font.bold: false
    }
}
