import QtQuick 2.12

Item {
    width: 110
    height: 99

    Rectangle {
        id: rectangle5
        x: 78
        y: 36
        width: 32
        height: 21
        color: "#313f5c"
    }

    Rectangle {
        id: rectangle4
        x: 46
        y: 36
        width: 32
        height: 21
        color: "#06286c"
    }

    Rectangle {
        id: rectangle3
        x: 55
        y: 61
        width: 55
        height: 38
        color: "#4d85f3"
    }

    Rectangle {
        id: rectangle
        x: 0
        y: 0
        width: 46
        height: 57
        color: "#486fba"
    }

    Rectangle {
        id: rectangle1
        x: 46
        y: 0
        width: 64
        height: 35
        color: "#2a457c"
    }

    Rectangle {
        id: rectangle7
        x: 48
        y: 47
        width: 13
        height: 8
        color: "#87a2da"

        MouseArea {
            id: mouseArea1
            anchors.fill: parent
        }
    }

    Rectangle {
        id: rectangle9
        x: 82
        y: 47
        width: 24
        height: 8
        color: "#87a2da"

        MouseArea {
            id: mouseArea3
            anchors.fill: parent
        }
    }

    Rectangle {
        id: rectangle2
        x: 0
        y: 61
        width: 55
        height: 38
        color: "#2a457c"

        Text {
            id: text2
            x: 63
            y: 12
            text: "remain"
            font.pixelSize: 12
        }

        Text {
            id: text1
            x: 5
            y: 12
            text: "Elapsed"
            font.pixelSize: 12
        }

        Text {
            id: text3
            x: 7
            y: -58
            width: 26
            height: 10
            text: "BPM"
            font.pixelSize: 7
            horizontalAlignment: Text.AlignHCenter
            font.bold: false
        }

        Text {
            id: text5
            x: 4
            y: -47
            width: 32
            height: 14
            text: "197.00"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            font.bold: true
        }

        Text {
            id: text4
            x: 50
            y: -53
            width: 31
            height: 20
            text: "Cm"
            font.pixelSize: 20
            font.bold: true
        }

        Text {
            id: text6
            x: 50
            y: -58
            text: "KEY"
            font.pixelSize: 7
        }

        Text {
            id: text7
            x: 55
            y: -24
            text: "KEY"
            font.pixelSize: 7
        }

        Text {
            id: text9
            x: 55
            y: -24
            text: "KEY"
            font.pixelSize: 7
        }

        Text {
            id: text12
            x: 86
            y: -24
            text: "KEY"
            font.pixelSize: 7
        }

        Text {
            id: text13
            x: 84
            y: -14
            text: "Cm V"
            font.pixelSize: 7
        }

    }






    Rectangle {
        id: rectangle6
        x: 87
        y: 11
        width: 17
        height: 17
        color: "#263a62"

        MouseArea {
            id: mouseArea
            anchors.fill: parent
        }
    }



    Image {
        id: lockOpen
        x: 90
        y: 13
        width: 11
        height: 12
        source: "../../../images/lock-open.svg"
        fillMode: Image.PreserveAspectFit
    }



    Rectangle {
        id: rectangle8
        x: 63
        y: 47
        width: 13
        height: 8
        color: "#87a2da"
    }



    Text {
        id: text10
        x: 63
        y: 43
        width: 13
        height: 17
        text: "+"
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
    }


    Text {
        id: text11
        x: 48
        y: 43
        width: 13
        height: 17
        text: "-"
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter

        MouseArea {
            id: mouseArea2
            anchors.fill: parent
        }
    }



}
