import "." as Skin
import QtQuick 2.12

Skin.Button {
    id: root

    required property string group
    required property string key
    property bool toggleable: false
    // M8: opt-in "driven by hardware" glow, off by default -- see
    // ControlKnob.qml's showHardwareIndicator for why this is Loader-gated
    // rather than an always-instantiated child.
    property bool showHardwareIndicator: false

    function toggle() {
        controlBehavior.toggleControl();
    }

    highlight: controlBehavior.isActive
    onPressed: {
        controlBehavior.pressPrimary();
    }
    onReleased: {
        controlBehavior.releasePrimary();
    }

    ControlProxyButtonBehavior {
        id: controlBehavior

        group: root.group
        key: root.key
        toggleable: root.toggleable
        handlePointerInput: false
    }
    Loader {
        active: root.showHardwareIndicator
        anchors.fill: parent
        sourceComponent: Rectangle {
            border.color: "#D9D9D9"
            border.width: 2
            color: "transparent"
            opacity: controlBehavior.hardwareDriven ? 0.8 : 0
            radius: 2

            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                }
            }
        }
    }
}
