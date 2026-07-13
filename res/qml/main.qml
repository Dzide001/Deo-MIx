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

    // minimumWidth/minimumHeight must be at least the sum of the deepest
    // Layout.minimumWidth/Height floors below (deck+mixer row: 400+400+400
    // + 2*24 spacing = 1248, plus 24 outer margins = 1272; three stacked
    // rows: 110+300+250 + 2*12 spacing + 24 margins = 708) -- Qt Quick
    // Layouts does not shrink children below their own minimums, it lets
    // them overflow past the allocated space (or, for a component whose
    // OWN declared minimum was set too low relative to its real internal
    // content, paint past its allocated slot into a neighbor) instead.
    // Also starts Maximized rather than a fixed 1600x1100 in plain
    // Windowed mode, so the UI always fills whatever screen space is
    // actually available instead of a size that can exceed it.
    color: Theme.backgroundColor
    height: 1100
    minimumHeight: 720
    minimumWidth: 1300
    visible: true
    visibility: Mixxx.Config.configStartInFullscreenKey ? Window.FullScreen : Window.Maximized
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

            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(300, root.height * 0.32)
            Layout.minimumHeight: 300
            spacing: 24

            // deck_A / mixer_module / deck_B are 39% / 22% / 39% of
            // deck_section's width in the spec. Deliberately NOT computed
            // as `deckMixerRow.width * 0.39` -- a child reading its own
            // parent RowLayout's width is a circular binding (the parent's
            // width can't be resolved without the children's hints, which
            // need the parent's width), which Qt Quick Layouts detects
            // ("recursive rearrange, aborting after two iterations") and
            // resolves to an unreliable value instead of erroring. Worse,
            // even once that's made non-circular, fillWidth children don't
            // shrink below their preferredWidth just because a sibling
            // needs more room -- Qt Quick Layouts distributes space to
            // fillWidth items, it doesn't renegotiate their ratio, so a
            // literal "39% of current width" value doesn't actually shrink
            // this deck when the mixer or the other deck needs to grow.
            // Plain preferredWidth *weights* (chosen to match 39/22/39 at a
            // representative ~1600px row width) sidestep both problems:
            // Qt Quick Layouts' own fillWidth algorithm distributes actual
            // available space using these as relative weights, so the
            // three panels always sum to exactly the row's width with no
            // overflow, at roughly the spec's ratio.
            Deo.DeckPanel {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 400
                Layout.preferredWidth: 624
                accentColor: Theme.deckAAccent
                effectUnitNumber: 1
                group: "[Channel1]"
                label: "DECK A"
            }
            Deo.MixerTabs {
                Layout.fillHeight: true
                Layout.fillWidth: true
                // >= AudioMixerPanel's own internal minimum sum (90+180+90
                // EQ/center/EQ = 360) and MasterPanel's (90+200+90 = 380).
                Layout.minimumWidth: 400
                Layout.preferredWidth: 352
                accentColorA: Theme.deckAAccent
                accentColorB: Theme.deckBAccent
            }
            Deo.DeckPanel {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 400
                Layout.preferredWidth: 624
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
