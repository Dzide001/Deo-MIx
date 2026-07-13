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

    color: Theme.backgroundColor
    height: 1100
    minimumHeight: 700
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Window-level vertical stack, matching Deo Pro dj_layout_spec.json's
        // root children: waveform_overview_section (14%), deck_section
        // (32%), browser_section (51%) — the spec's own top_bar (3%) isn't
        // built (the Preferences button substitutes for it), so these three
        // percentages are applied directly against the window height rather
        // than renormalized.
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
            id: deckMixerRow

            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(300, root.height * 0.32)
            Layout.minimumHeight: 300
            spacing: 24

            // deck_A / mixer_module / deck_B are 39% / 22% / 39% of
            // deck_section's width in the spec -- previously the mixer used
            // a fixed 400px, which grew to a disproportionate share of the
            // row (up to ~30%+) as the window narrowed toward its minimum
            // width, since the decks (percentage-flexed internally) shrank
            // faster than the mixer's fixed floor.
            Deo.DeckPanel {
                Layout.fillHeight: true
                Layout.minimumWidth: 480
                Layout.preferredWidth: deckMixerRow.width * 0.39
                accentColor: Theme.deckAAccent
                effectUnitNumber: 1
                group: "[Channel1]"
                label: "DECK A"
            }
            Deo.MixerTabs {
                Layout.fillHeight: true
                Layout.minimumWidth: 380
                Layout.preferredWidth: deckMixerRow.width * 0.22
                accentColorA: Theme.deckAAccent
                accentColorB: Theme.deckBAccent
            }
            Deo.DeckPanel {
                Layout.fillHeight: true
                Layout.minimumWidth: 480
                Layout.preferredWidth: deckMixerRow.width * 0.39
                accentColor: Theme.deckBAccent
                effectUnitNumber: 2
                group: "[Channel2]"
                label: "DECK B"
                mirrored: true
            }
        }
        // M7: browser_section is a permanent, always-visible window-level
        // row in the spec (51% of window height, sibling of deck_section),
        // not a toggleable overlay -- it was built as a toggle in the first
        // pass without checking this file, which was wrong.
        Library {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(250, root.height * 0.51)
            Layout.minimumHeight: 250
            clip: true
        }
    }
}
