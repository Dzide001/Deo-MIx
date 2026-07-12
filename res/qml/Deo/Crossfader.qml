import QtQuick 2.12
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import "../Theme"

// Horizontal crossfader, [Master],crossfader. Same MixxxControls.Slider
// approach as ChannelFader.qml/EffectMetaSlider.qml, avoiding Skin.Fader's
// broken bar.margin. Linear/additive curve is Mixxx's actual stock
// default (MIXXX_XFADER_ADDITIVE in dlgprefmixer.cpp); changing curves is
// a Preferences-level setting, not something this control needs to know
// about.
Item {
    id: root

    Mixxx.ControlProxy {
        id: crossfaderControl

        group: "[Master]"
        key: "crossfader"
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        height: 4
        radius: 2
        color: Theme.knobBackgroundColor
    }
    // Center detent.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: 2
        height: 14
        color: Theme.midGray3
        z: 1
    }
    MixxxControls.Slider {
        id: fader

        anchors.fill: parent
        from: -1
        live: true
        orientation: Qt.Horizontal
        to: 1
        value: crossfaderControl.value

        handle: Rectangle {
            width: 14
            height: 22
            radius: 2
            border.color: Theme.darkGray3
            border.width: 1
            color: Theme.white

            x: fader.leftPadding + fader.visualPosition * fader.availableWidth - width / 2
            y: (fader.height - height) / 2
        }

        onMoved: newValue => {
            crossfaderControl.value = newValue;
        }
    }
}
