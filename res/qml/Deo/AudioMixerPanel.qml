import QtQuick 2.12
import QtQuick.Layouts
import "." as Deo
import ".." as Skin

// M5 AUDIO tab. Proportions match "DEo audio_mixer_panel_spec.json":
// eq_column_left 26% / mixer_center_panel 49% / eq_column_right 25% of
// this row's width. EQ columns reuse stock Skin.EqColumn as-is (pure
// knob/combobox, no Skin.Fader involved, so unaffected by the
// bar.margin issue). Crossfader lives in MixerTabs.qml now (persistent
// across tabs, not AUDIO-only).
RowLayout {
    id: root

    required property color accentColorA
    required property color accentColorB

    spacing: 0

    Skin.EqColumn {
        Layout.preferredWidth: root.width * 0.26
        Layout.minimumWidth: 90
        Layout.fillHeight: true
        group: "[Channel1]"
    }
    RowLayout {
        Layout.preferredWidth: root.width * 0.49
        Layout.minimumWidth: 180
        Layout.fillHeight: true
        spacing: 4

        Deo.ChannelStrip {
            Layout.fillWidth: true
            Layout.fillHeight: true
            accentColor: root.accentColorA
            group: "[Channel1]"
        }
        Deo.MixerVuMeters {
            Layout.fillHeight: true
            Layout.preferredWidth: 24
        }
        Deo.ChannelStrip {
            Layout.fillWidth: true
            Layout.fillHeight: true
            accentColor: root.accentColorB
            group: "[Channel2]"
        }
    }
    Skin.EqColumn {
        Layout.preferredWidth: root.width * 0.25
        Layout.minimumWidth: 90
        Layout.fillHeight: true
        group: "[Channel2]"
    }
}
