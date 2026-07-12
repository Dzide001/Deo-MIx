import "Deo" as Deo
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "Theme"

// M1: jog wheel (VINYL/SLIP) + CUE/Play/SYNC. M2 adds the pitch fader and
// loop section. FX rack, pads, mixer, waveform, and the library/browser
// are still out of scope until later milestones.
ApplicationWindow {
    id: root

    color: Theme.backgroundColor
    height: 700
    minimumHeight: 560
    minimumWidth: 760
    visible: true
    visibility: Mixxx.Config.configStartInFullscreenKey ? Window.FullScreen : Window.Windowed
    width: 1040

    Mixxx.ControlProxy {
        group: "[App]"
        key: "num_decks"

        onInitializedChanged: {
            value = 2;
        }
    }

    // Not part of the M1 spec's UI scope, but needed so a fresh install can
    // reach Sound Hardware preferences (no audio output device is selected
    // by default) — otherwise CUE/Play/SYNC "work" but nothing is audible
    // and there's no way to fix that from this stripped-down view.
    Button {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        text: "⚙ Preferences"
        z: 10

        onClicked: {
            Mixxx.PreferencesDialog.show();
        }
    }

    RowLayout {
        anchors.centerIn: parent
        spacing: 48

        Deo.DeckPanel {
            Layout.preferredWidth: 340
            accentColor: Theme.deckAAccent
            group: "[Channel1]"
            label: "DECK A"
        }
        Deo.DeckPanel {
            Layout.preferredWidth: 340
            accentColor: Theme.deckBAccent
            group: "[Channel2]"
            label: "DECK B"
            mirrored: true
        }
    }
}
