import QtQuick 2.12
import QtQuick.Layouts
import "." as Deo
import ".." as Skin

// M5 AUDIO tab: EQ columns (reusing stock Skin.EqColumn as-is — it's
// pure-knob/combobox, no Skin.Fader involved, so it isn't affected by
// the bar.margin issue) flanking each deck's channel strip, crossfader
// below.
ColumnLayout {
    id: root

    required property color accentColorA
    required property color accentColorB

    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 8

        Skin.EqColumn {
            Layout.preferredWidth: 42
            group: "[Channel1]"
        }
        Deo.ChannelStrip {
            Layout.fillHeight: true
            accentColor: root.accentColorA
            group: "[Channel1]"
        }
        Deo.ChannelStrip {
            Layout.fillHeight: true
            accentColor: root.accentColorB
            group: "[Channel2]"
        }
        Skin.EqColumn {
            Layout.preferredWidth: 42
            group: "[Channel2]"
        }
    }
    Deo.Crossfader {
        Layout.fillWidth: true
        Layout.preferredHeight: 26
    }
}
