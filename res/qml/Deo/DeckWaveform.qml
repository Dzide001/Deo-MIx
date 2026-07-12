import QtQuick 2.12
import QtQuick.Layouts
import ".." as Skin

// M6: main scrolling waveform + whole-track overview strip. Both reuse
// Mixxx's own native, GPU-accelerated waveform renderer components
// (MixxxControls.WaveformDisplay/WaveformOverview, the exact custom
// QQuickItem approach the spec anticipated needing to build from
// scratch) via the stock res/qml/WaveformDisplay.qml and
// WaveformOverview.qml compositions — no new rendering code needed.
// Beat markers, cue/loop/intro/outro marks, played/unplayed shading,
// zoom (mouse wheel), scratch/bend mouse handling, and the 30-second
// end-of-track warning all come from those components already; hotcue
// markers were the one gap and are added directly to
// res/qml/WaveformDisplay.qml (8 hotcues, matching the count used
// elsewhere in this skin).
ColumnLayout {
    id: root

    required property string group
    required property color accentColor

    spacing: 2

    Rectangle {
        Layout.fillWidth: true
        height: 2
        color: root.accentColor
    }
    Skin.WaveformDisplay {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 60
        group: root.group
    }
    Skin.WaveformOverview {
        Layout.fillWidth: true
        Layout.preferredHeight: 20
        group: root.group
    }
}
