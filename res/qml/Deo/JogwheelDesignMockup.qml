import QtQuick 2.12

Item {
    width: 221
    height: 234

    Rectangle {
        id: rectangle3
        x: 198
        y: 8
        width: 15
        height: 12
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle1
        x: 43
        y: 201
        width: 42
        height: 25
        color: "#0f2e3f"
        radius: 8
    }

    Text {
        id: text5
        x: 52
        y: 206
        color: "#91b1b4"
        text: "CUE"
        font.pixelSize: 12
        styleColor: "#a0d4e0"
        rotation: 0
        font.bold: false
    }

    Rectangle {
        id: rectangle2
        x: 92
        y: 201
        width: 42
        height: 25
        color: "#0f2e3f"
        radius: 8
    }

    Text {
        id: text6
        x: 98
        y: 206
        color: "#91b1b4"
        text: "PLAY"
        font.pixelSize: 12
        styleColor: "#a0d4e0"
        rotation: 0
        font.bold: false
    }

    Rectangle {
        id: rectangle4
        x: 140
        y: 201
        width: 42
        height: 25
        color: "#0f2e3f"
        radius: 8
    }

    Text {
        id: text7
        x: 145
        y: 206
        color: "#91b1b4"
        text: "SYNC"
        font.pixelSize: 12
        styleColor: "#a0d4e0"
        rotation: 0
        font.bold: false
    }

    Rectangle {
        id: rectangle6
        x: 198
        y: 23
        width: 15
        height: 12
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle7
        x: 13
        y: 8
        width: 16
        height: 13
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle8
        x: 13
        y: 25
        width: 16
        height: 16
        color: "#0f2e3f"
        radius: 7
    }

    Image {
        id: gearSvgrepoCom
        x: 15
        y: 27
        width: 12
        height: 12
        source: "../../../images/gear-svgrepo-com.svg"
        fillMode: Image.PreserveAspectFit
    }

    Rectangle {
        id: rectangle
        x: 21
        y: 15
        width: 180
        height: 180
        color: "#022331"
        radius: 90
    }


}
