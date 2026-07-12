import QtQuick 2.12
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import "../Theme"

// Horizontal metaknob slider for one FX slot, matching the reference's
// slider column (stock Skin.EffectSlot uses a knob for this instead).
// Built directly on MixxxControls.Slider rather than Skin.ControlFader,
// same reason as PitchFader.qml: Skin.Fader's bar.margin doesn't resolve
// once Mixxx.Controls is compiled into a QML plugin in this pinned
// commit, so this uses its own from-scratch fill/handle visual.
Item {
    id: root

    required property string group
    required property color accentColor

    Mixxx.ControlProxy {
        id: metaControl

        group: root.group
        key: "meta"
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 4
        radius: 2
        color: Theme.knobBackgroundColor
    }
    Rectangle {
        x: 0
        y: parent.height / 2 - 2
        width: slider.leftPadding + slider.visualPosition * slider.availableWidth
        height: 4
        radius: 2
        color: root.accentColor
    }
    MixxxControls.Slider {
        id: slider

        anchors.fill: parent
        from: 0
        live: true
        orientation: Qt.Horizontal
        value: metaControl.parameter

        handle: Rectangle {
            width: 10
            height: 18
            radius: 2
            border.color: Theme.darkGray3
            border.width: 1
            color: Theme.white

            x: slider.leftPadding + slider.visualPosition * slider.availableWidth - width / 2
            y: (slider.height - height) / 2
        }

        onMoved: newValue => {
            metaControl.parameter = newValue;
        }
    }
}
