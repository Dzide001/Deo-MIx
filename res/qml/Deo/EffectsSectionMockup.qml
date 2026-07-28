import QtQuick 2.12
import QtQuick.Controls 2.12

Item {
    width: 252
    height: 108
    scale: 1

    Rectangle {
        id: rectangle2
        x: 3
        y: 5
        width: 18
        height: 99
        color: "#0f2e3f"
    }

    Text {
        id: text1
        x: -5
        y: 47
        color: "#91b1b4"
        text: "PADS"
        font.pixelSize: 12
        rotation: 270
        font.bold: true
    }

    Rectangle {
        id: rectangle5
        x: 58
        y: 5
        width: 82
        height: 27
        color: rectangle5.active ? "#2a698b" : "#0f2e3f"

        property bool active: false

            MouseArea {
                id: mouseArearec1
                anchors.fill: parent
                anchors.rightMargin: 0
                onClicked: rectangle5.active = !rectangle5.active
            }
    }

    Text {
        id: text3
        x: 62
        y: 12
        color: "#91b1b4"
        text: "BACKSPIN"
        font.pixelSize: 10
        rotation: 0
        font.bold: false
    }

    Text {
        id: text4
        x: 130
        y: 12
        color: "#91b1b4"
        text: "V"
        font.pixelSize: 10
        rotation: 0
        font.bold: false
    }

    Rectangle {
        id: rectangle6
        x: 58
        y: 71
        width: 82
        height: 27
        color: rectangle6.active ? "#2a698b" : "#0f2e3f"

        property bool active: false

            MouseArea {
                id: mouseArearec3
                anchors.fill: parent
                onClicked: rectangle6.active = !rectangle6.active
            }
    }

    Text {
        id: text5
        x: 62
        y: 78
        color: "#91b1b4"
        text: "WAHWAH"
        font.pixelSize: 10
        rotation: 0
        font.bold: false
    }

    Text {
        id: text6
        x: 130
        y: 78
        color: "#91b1b4"
        text: "V"
        font.pixelSize: 10
        rotation: 0
        font.bold: false
    }

    Rectangle {
        id: rectangle7
        x: 58
        y: 38
        width: 82
        height: 27
        color: rectangle7.active ? "#2a698b" : "#0f2e3f"

        property bool active: false

            MouseArea {
                id: mouseArearec2
                anchors.fill: parent
                anchors.rightMargin: 0
                onClicked: rectangle7.active = !rectangle7.active
            }
    }

    Text {
        id: text7
        x: 62
        y: 46
        color: "#91b1b4"
        text: "REVERB"
        font.pixelSize: 10
        rotation: 0
        font.bold: false
    }

    Text {
        id: text8
        x: 130
        y: 46
        color: "#91b1b4"
        text: "V"
        font.pixelSize: 10
        rotation: 0
        font.bold: false
    }

    Rectangle {
        id: rectangle9
        x: 224
        y: 10
        width: 24
        height: 17
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle12
        x: 224
        y: 41
        width: 24
        height: 19
        color: "#0f2e3f"
    }

    Rectangle {
        id: rectangle15
        x: 224
        y: 72
        width: 24
        height: 19
        color: "#0f2e3f"
    }



    Image {
        id: gear2
        x: 231
        y: 76
        width: 11
        height: 11
        source: "../../../images/gear.png"
        fillMode: Image.PreserveAspectFit

        property bool active: false

        MouseArea {
            id: mouseArea2
            anchors.fill: parent
        }
    }

    Image {
        id: gear
        x: 231
        y: 13
        width: 11
        height: 11
        source: "../../../images/gear.png"
        fillMode: Image.PreserveAspectFit

        property bool active: false

        MouseArea {
            id: mouseArea
            anchors.fill: parent
        }
    }
    Image {
        id: gear1
        x: 231
        y: 46
        width: 11
        height: 11
        source: "../../../images/gear.png"
        fillMode: Image.PreserveAspectFit

        property bool active: false

        MouseArea {
            id: mouseArea1
            anchors.fill: parent
        }
    }

    Slider {
        id: slider
        x: 145
        y: 9
        width: 70
        from: 0
        to: 1
        height: 19
        value: 0.0

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 6
            radius: 3
            color: "#0f2e3f"
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 8
            height: 17
            radius: 4
            color: "#2a698b"
        }
    }

    Slider {
        id: slider1
        x: 145
        y: 43
        width: 70
        height: 19
        value: 0
        to: 1
        from: 0

        background: Rectangle {
            x: slider1.leftPadding
            y: slider1.topPadding + slider1.availableHeight / 2 - height / 2
            width: slider1.availableWidth
            height: 6
            radius: 3
            color: "#0f2e3f"
        }

        handle: Rectangle {
            x: slider1.leftPadding + slider1.visualPosition * (slider1.availableWidth - width)
            y: slider1.topPadding + slider1.availableHeight / 2 - height / 2
            width: 8
            height: 17
            radius: 4
            color: "#2a698b"
        }
    }

    Slider {
        id: slider2
        x: 145
        y: 75
        width: 70
        height: 19
        value: 0
        to: 1
        from: 0

        background: Rectangle {
            x: slider2.leftPadding
            y: slider2.topPadding + slider2.availableHeight / 2 - height / 2
            width: slider2.availableWidth
            height: 6
            radius: 3
            color: "#0f2e3f"
        }

        handle: Rectangle {
            x: slider2.leftPadding + slider2.visualPosition * (slider2.availableWidth - width)
            y: slider2.topPadding + slider2.availableHeight / 2 - height / 2
            width: 8
            height: 17
            radius: 4
            color: "#2a698b"
        }
    }

    Rectangle {
        id: rectangle
        x: 31
        y: 5
        width: 17
        height: 27
        color: rectangle.active ? "#2a698b" : "#0f2e3f"
        radius: 6

        property bool active: false

        MouseArea {
            id: mouseArea3
            anchors.fill: parent
            onClicked: rectangle.active = !rectangle.active
        }
    }

    Rectangle {
        id: rectangle1
        x: 31
        y: 38
        width: 17
        height: 27
        color: rectangle1.active ? "#2a698b" : "#0f2e3f"
        radius: 6

        property bool active: false

        MouseArea {
            id: mouseArea4
            anchors.fill: parent
            onClicked: rectangle1.active = !rectangle1.active
        }
    }

    Rectangle {
        id: rectangle3
        x: 31
        y: 71
        width: 17
        height: 27
        color: rectangle3.active ? "#2a698b" : "#0f2e3f"
        radius: 6

        property bool active: false

        MouseArea {
            id: mouseArea5
            anchors.fill: parent
            onClicked: rectangle3.active = !rectangle3.active
        }
    }


}
