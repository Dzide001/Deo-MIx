import QtQuick 2.12
import "." as Deo

// M5 AUDIO tab. Rebuilt as a plain Item with the literal coordinates
// from the user's reference mockup (MixerDesignMockupA.qml), matching
// this panel's own real allocated 297x345 zone (the mockup's full
// 297x378 canvas minus its 33px tab row) exactly: EQ column A | fader
// strip A | VU lane A | center divider | VU lane B | fader strip B |
// EQ column B. Crossfader lives in MixerTabs.qml (persistent across
// tabs, not AUDIO-only).
Item {
    id: root

    required property color accentColorA
    required property color accentColorB

    width: 297
    height: 345

    Deo.EqColumn {
        x: 0
        y: 0
        group: "[Channel1]"
        rightSide: false
    }
    Deo.ChannelStrip {
        x: 66
        y: 0
        accentColor: root.accentColorA
        group: "[Channel1]"
    }
    Deo.MixerVuMeters {
        x: 121
        y: 0
        group: "[Channel1]"
    }
    Rectangle {
        x: 148
        y: 0
        width: 1
        height: 345
        color: "#842121"
    }
    Deo.MixerVuMeters {
        x: 149
        y: 0
        group: "[Channel2]"
    }
    Deo.ChannelStrip {
        x: 176
        y: 0
        accentColor: root.accentColorB
        group: "[Channel2]"
    }
    Deo.EqColumn {
        x: 231
        y: 0
        group: "[Channel2]"
        rightSide: true
    }
}
