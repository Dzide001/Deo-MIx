import "Deo" as Deo
import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "Theme"

// M1: jog wheel (VINYL/SLIP) + CUE/Play/SYNC. M2 adds the pitch fader and
// loop section. M3 adds the per-deck FX rack. M5 adds the mixer (AUDIO +
// MASTER tabs; VIDEO disabled pending M12, SCRATCH out of scope). Pads
// and the library/browser are still out of scope until later milestones.
ApplicationWindow {
    id: root

    // M7: no spatial spec was provided for the library/browser, so it's a
    // toggleable panel docked at the bottom rather than squeezed into the
    // already-full deck+mixer layout -- growing the window by exactly the
    // panel's own height when opened, so the deck/mixer/waveform rows above
    // it render identically to before regardless of toggle state.
    property bool libraryOpen: false
    readonly property int libraryPanelHeight: 340
    readonly property int collapsedHeight: 760

    color: Theme.backgroundColor
    height: collapsedHeight
    minimumHeight: 620
    minimumWidth: 1300
    visible: true
    visibility: Mixxx.Config.configStartInFullscreenKey ? Window.FullScreen : Window.Windowed
    width: 1600

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
    // M7: Search/Tracks/Playlists/Crates/Computer sidebar + track table,
    // toggled rather than always-on since there's no room for a
    // permanently-docked library alongside the existing deck/mixer layout.
    Button {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        anchors.rightMargin: 132
        text: root.libraryOpen ? "📁 Hide Library" : "📁 Library"
        z: 10

        onClicked: {
            root.libraryOpen = !root.libraryOpen;
            root.height = root.libraryOpen ? (root.collapsedHeight + root.libraryPanelHeight) : root.collapsedHeight;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // M6: scrolling waveform spans the full window width above the
        // deck section, matching Deo Pro dj_layout_spec.json's
        // waveform_overview_section (a window-level row, 14% of window
        // height, sibling of deck_section — not nested inside each deck's
        // own column). Per the VirtualDJ-style reference screenshot, the
        // two decks' scrolling lanes are STACKED (Deck A on top, Deck B
        // below), each spanning the full window width, rather than placed
        // side by side in half-width columns.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(110, root.height * 0.14)
            Layout.minimumHeight: 110
            spacing: 2

            Deo.DeckWaveform {
                Layout.fillWidth: true
                Layout.fillHeight: true
                accentColor: Theme.deckAAccent
                group: "[Channel1]"
            }
            Deo.DeckWaveform {
                Layout.fillWidth: true
                Layout.fillHeight: true
                accentColor: Theme.deckBAccent
                group: "[Channel2]"
            }
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            spacing: 24

            Deo.DeckPanel {
                accentColor: Theme.deckAAccent
                effectUnitNumber: 1
                group: "[Channel1]"
                label: "DECK A"
            }
            Deo.MixerTabs {
                Layout.preferredWidth: 400
                Layout.minimumWidth: 380
                Layout.preferredHeight: 420
                accentColorA: Theme.deckAAccent
                accentColorB: Theme.deckBAccent
            }
            Deo.DeckPanel {
                accentColor: Theme.deckBAccent
                effectUnitNumber: 2
                group: "[Channel2]"
                label: "DECK B"
                mirrored: true
            }
        }
        Library {
            Layout.fillWidth: true
            Layout.preferredHeight: root.libraryOpen ? root.libraryPanelHeight : 0
            clip: true
            visible: root.libraryOpen
        }
    }
}
