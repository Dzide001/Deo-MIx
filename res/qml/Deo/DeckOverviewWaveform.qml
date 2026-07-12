import QtQuick 2.12
import ".." as Skin

// Whole-track "static" overview strip, living inside the deck panel
// itself (matching deckA_mini_waveform in Deo Pro dj_layout_spec.json —
// a small preview strip inside each deck, distinct from the shared
// full-width scrolling waveform in main.qml). Reuses Mixxx's own native
// overview renderer via the stock res/qml/WaveformOverview.qml.
Item {
    id: root

    required property string group

    implicitHeight: 18

    Skin.WaveformOverview {
        anchors.fill: parent
        group: root.group
    }
}
