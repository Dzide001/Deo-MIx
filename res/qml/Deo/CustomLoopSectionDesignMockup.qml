import QtQuick 2.12

Item {
    width: 139
    height: 108

    Rectangle {
        id: rectangle1
        x: 86
        y: 57
        width: 51
        height: 42
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle2
        x: 32
        y: 57
        width: 51
        height: 42
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle3
        x: 64
        y: 9
        width: 41
        height: 42
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle4
        x: 32
        y: 9
        width: 29
        height: 42
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle5
        x: 108
        y: 9
        width: 29
        height: 42
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle6
        x: 3
        y: 3
        width: 25
        height: 99
        color: "#0f2e3f"
    }

    Text {
        id: text1
        x: -3
        y: 47
        color: "#91b1b4"
        text: "LOOP"
        font.pixelSize: 12
        styleColor: "#a0d4e0"
        rotation: 270
        font.bold: true
    }

    Text {
        id: text2
        x: 43
        y: 23
        color: "#91b1b4"
        text: "<"
        font.pixelSize: 12
        styleColor: "#a0d4e0"
        rotation: 0
        font.bold: true
    }

    Text {
        id: text3
        x: 119
        y: 23
        color: "#91b1b4"
        text: ">"
        font.pixelSize: 12
        styleColor: "#a0d4e0"
        rotation: 0
        font.bold: true
    }

    Text {
        id: text4
        x: 51
        y: 71
        color: "#91b1b4"
        text: "IN"
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        styleColor: "#a0d4e0"
        rotation: 0
        font.bold: false
    }

    Text {
        id: text5
        x: 99
        y: 71
        color: "#91b1b4"
        text: "OUT"
        font.pixelSize: 12
        styleColor: "#a0d4e0"
        rotation: 0
        font.bold: false
    }

}
