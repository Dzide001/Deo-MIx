import QtQuick 2.12
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import "../Theme"

// Vertical channel-volume fader. Built directly on MixxxControls.Slider
// rather than Skin.ControlFader/Skin.Fader for the same reason as
// PitchFader.qml/EffectMetaSlider.qml: Skin.Fader's bar.margin doesn't
// resolve once Mixxx.Controls is compiled into a QML plugin in this
// pinned commit.
Item {
    id: root

    required property string group

    Mixxx.ControlProxy {
        id: volumeControl

        group: root.group
        key: "volume"
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 4
        radius: 2
        color: Theme.knobBackgroundColor
    }
    // Unity-gain tick near the top of the travel, matching Mixxx's
    // default volume curve where ~0.78 parameter is roughly 0 dB.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: fader.topPadding + (1 - 0.78) * fader.availableHeight - 1
        width: 14
        height: 2
        color: Theme.midGray3
        z: 1
    }
    Rectangle {
        readonly property real handleY: fader.topPadding + fader.visualPosition * fader.availableHeight

        x: parent.width / 2 - 2
        y: handleY
        width: 4
        height: parent.height - handleY
        color: Theme.volumeSliderBarColor
        z: 1
    }
    MixxxControls.Slider {
        id: fader

        anchors.fill: parent
        from: 0
        live: true
        orientation: Qt.Vertical
        value: volumeControl.parameter

        handle: Rectangle {
            width: 24
            height: 10
            radius: 2
            border.color: Theme.darkGray3
            border.width: 1
            color: Theme.white

            x: (fader.width - width) / 2
            y: fader.topPadding + fader.visualPosition * fader.availableHeight - height / 2
        }

        onMoved: newValue => {
            volumeControl.parameter = newValue;
        }
    }
}
