import QtQuick 2.12
import ".." as Skin

// M6: one deck's scrolling waveform lane. Meant to be stacked full-width
// (Deck A's lane on top, Deck B's on the bottom, both spanning the full
// window width) rather than placed side by side — per the reference
// screenshot, the scrolling waveform is one shared full-width area split
// into top/bottom halves by deck, not two half-width columns.
//
// Reuses Mixxx's own native, GPU-accelerated waveform renderer
// (MixxxControls.WaveformDisplay, the exact custom QQuickItem approach
// the M6 spec anticipated needing to build from scratch) via the stock
// res/qml/WaveformDisplay.qml composition — no new rendering code
// needed. Beat markers, cue/loop/intro/outro/hotcue marks, played/
// unplayed shading, zoom (mouse wheel), scratch/bend mouse handling, and
// the 30-second end-of-track warning all come from that component.
Item {
    id: root

    required property string group
    required property color accentColor

    // Thin accent-colored edge to tell the two lanes apart at a glance,
    // matching the small colored indicator on the reference screenshot's
    // left edge.
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 3
        color: root.accentColor
    }
    Skin.WaveformDisplay {
        anchors.fill: parent
        anchors.leftMargin: 5
        group: root.group
    }
}
