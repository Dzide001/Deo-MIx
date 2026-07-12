import QtQuick 2.12
import QtQuick.Layouts
import ".." as Skin

// Shared VU meter cluster between the two decks' faders, matching
// mixer_vu_meter's position in the spec (a single group centered between
// gain_fader_A and gain_fader_B, not per-fader flanking meters). Shows
// stereo L/R for both channels rather than collapsing to mono, since
// that's more informative and the underlying vu_meter_left/right COs
// already exist per-channel.
RowLayout {
    id: root

    spacing: 3

    Skin.VuMeter {
        Layout.fillHeight: true
        group: "[Channel1]"
        key: "vu_meter_left"
        width: 4
    }
    Skin.VuMeter {
        Layout.fillHeight: true
        group: "[Channel1]"
        key: "vu_meter_right"
        width: 4
    }
    Skin.VuMeter {
        Layout.fillHeight: true
        group: "[Channel2]"
        key: "vu_meter_left"
        width: 4
    }
    Skin.VuMeter {
        Layout.fillHeight: true
        group: "[Channel2]"
        key: "vu_meter_right"
        width: 4
    }
}
