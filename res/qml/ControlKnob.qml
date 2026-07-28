import "." as Skin
import Mixxx 1.0 as Mixxx
import QtQuick 2.12

Skin.Knob {
    id: root

    property alias group: control.group
    property alias key: control.key
    // M8: opt-in "driven by hardware" ring glow, off by default so the
    // hundreds of existing ControlKnob instances across the skin (hotcues,
    // library trees, etc.) don't each pay for an extra Item -- only
    // instantiated via the Loader below for call sites that ask for it.
    property bool showHardwareIndicator: false

    value: control.parameter

    onTurned: control.parameter = value

    Mixxx.ControlProxy {
        id: control
    }
    TapHandler {
        onDoubleTapped: control.reset()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton

        onTapped: control.reset()
    }
    Loader {
        active: root.showHardwareIndicator
        anchors.centerIn: parent
        sourceComponent: Rectangle {
            border.color: "#D9D9D9"
            border.width: 2
            color: "transparent"
            height: root.width + 6
            opacity: control.hardwareDriven ? 0.8 : 0
            radius: height / 2
            width: root.width + 6

            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                }
            }
        }
    }
}
