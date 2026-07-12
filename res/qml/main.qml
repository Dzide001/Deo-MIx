import "Deo" as Deo
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "Theme"

// Milestone 1: deck + transport only (jog wheel, CUE/Play/SYNC) for two
// mirrored decks. Pitch fader, loops, FX rack, pads, mixer, waveform, and
// the library/browser are explicitly out of scope until later milestones
// (see milestone_1_deck_transport_spec.md).
ApplicationWindow {
    id: root

    color: Theme.backgroundColor
    height: 640
    minimumHeight: 480
    minimumWidth: 640
    visible: true
    visibility: Mixxx.Config.configStartInFullscreenKey ? Window.FullScreen : Window.Windowed
    width: 900

    Mixxx.ControlProxy {
        group: "[App]"
        key: "num_decks"

        onInitializedChanged: {
            value = 2;
        }
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 48

        Deo.DeckPanel {
            Layout.preferredWidth: 300
            accentColor: Theme.deckAAccent
            group: "[Channel1]"
            label: "DECK A"
        }
        Deo.DeckPanel {
            Layout.preferredWidth: 300
            accentColor: Theme.deckBAccent
            group: "[Channel2]"
            label: "DECK B"
            mirrored: true
        }
    }
}
