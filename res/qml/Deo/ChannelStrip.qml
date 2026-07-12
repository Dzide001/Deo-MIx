import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// GAIN knob, VU meter, channel fader, PFL toggle — the center column of a
// deck's mixer strip. accentColor drives the PFL "engaged" color per
// M5's acceptance criteria; the VU meter itself intentionally does not
// use it (clip/peak coloring is universal, not per-deck branding).
ColumnLayout {
    id: root

    required property string group
    required property color accentColor

    spacing: 6

    Label {
        Layout.alignment: Qt.AlignHCenter
        color: Theme.deckTextSecondary
        font.family: Theme.fontFamily
        font.pixelSize: 10
        text: "GAIN"
    }
    Skin.ControlKnob {
        Layout.alignment: Qt.AlignHCenter
        color: Theme.gainKnobColor
        group: root.group
        height: 36
        key: "pregain"
        width: 36
    }
    RowLayout {
        Layout.fillHeight: true
        Layout.alignment: Qt.AlignHCenter
        spacing: 4

        Skin.VuMeter {
            Layout.fillHeight: true
            group: root.group
            key: "vu_meter_left"
            width: 4
        }
        Deo.ChannelFader {
            Layout.fillHeight: true
            Layout.preferredWidth: 34
            group: root.group
        }
        Skin.VuMeter {
            Layout.fillHeight: true
            group: root.group
            key: "vu_meter_right"
            width: 4
        }
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
