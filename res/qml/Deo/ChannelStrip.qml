import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// GAIN knob, channel fader, headphone-cue toggle — one deck's mixer
// strip. VU meters live in a separate shared cluster between the two
// strips (see AudioMixerPanel.qml / MixerVuMeters.qml), matching the
// spec's mixer_vu_meter group rather than flanking each fader
// individually. accentColor drives the PFL "engaged" color per M5's
// acceptance criteria.
ColumnLayout {
    id: root

    required property string group
    required property color accentColor

    spacing: 6

    Skin.ControlKnob {
        Layout.alignment: Qt.AlignHCenter
        color: Theme.gainKnobColor
        group: root.group
        height: 36
        key: "pregain"
        width: 36
    }
    Label {
        Layout.alignment: Qt.AlignHCenter
        color: Theme.deckTextSecondary
        font.family: Theme.fontFamily
        font.pixelSize: 10
        text: "GAIN"
    }
    Deo.ChannelFader {
        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.preferredWidth: 34
        Layout.alignment: Qt.AlignHCenter
        group: root.group
    }
    Skin.ControlButton {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 34
        Layout.preferredHeight: 26
        activeColor: root.accentColor
        group: root.group
        key: "pfl"
        text: "🎧"
        toggleable: true
    }
}
