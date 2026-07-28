import QtQuick 2.12
import QtQuick.Controls 2.12
import "." as Deo
import ".." as Skin
import "../Theme"

// GAIN knob, channel fader, headphone-cue toggle -- one deck's mixer
// strip. Rebuilt as a plain Item with the literal coordinates from the
// user's reference mockup (MixerDesignMockupA.qml), matching this
// strip's own real allocated 55x345 zone within the mockup's 297x378
// canvas exactly. VU meters live in a separate shared cluster (see
// AudioMixerPanel.qml / MixerVuMeters.qml).
Item {
    id: root

    required property string group
    required property color accentColor

    width: 55
    height: 345

    Skin.ControlKnob {
        x: 6
        y: 8
        width: 42
        height: 42
        color: Theme.gainKnobColor
        group: root.group
        key: "pregain"
    }
    Label {
        x: 0
        y: 52
        width: 55
        color: Theme.deckTextSecondary
        font.family: Theme.fontFamily
        font.pixelSize: 10
        horizontalAlignment: Text.AlignHCenter
        text: "GAIN"
    }
    Deo.ChannelFader {
        x: 10
        y: 75
        width: 34
        height: 174
        group: root.group
    }
    Skin.ControlButton {
        x: 6
        y: 259
        width: 42
        height: 42
        activeColor: root.accentColor
        group: root.group
        key: "pfl"
        text: "🎧"
        toggleable: true
    }
}
