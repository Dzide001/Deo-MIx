import QtQuick 2.12
import QtQuick.Layouts
import "." as Deo
import ".." as Skin

// M5: AUDIO/VIDEO/MASTER tab shell (SCRATCH removed entirely — not a
// defined feature, deprioritized per discussion). Only AUDIO and MASTER
// have real content; VIDEO is disabled (blocked on M12 — no video engine
// exists). StackLayout keeps every tab's contents alive rather than
// recreating them via a Loader, so switching away and back doesn't lose
// control state (acceptance criterion #6).
//
// The crossfader is persistent across tabs rather than living inside
// AudioMixerPanel: both deo_master_panel_spec.json and
// "DEo audio_mixer_panel_spec.json" show a fader in the identical
// position/style at the bottom of the mixer regardless of which tab is
// selected, matching how a physical mixer's crossfader doesn't change
// based on what's shown on a screen above it.
ColumnLayout {
    id: root

    // Defensive containment: AudioMixerPanel/MasterPanel's own internal
    // minimums (EQ/center/EQ columns) can exceed whatever slot this gets
    // allocated if that mismatch ever recurs -- clip instead of visually
    // bleeding into the deck panel rendered next to it.
    clip: true

    required property color accentColorA
    required property color accentColorB

    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        spacing: 0

        Skin.Button {
            Layout.fillWidth: true
            checkable: true
            checked: tabStack.currentIndex === 0
            text: "AUDIO"

            onClicked: tabStack.currentIndex = 0
        }
        Skin.Button {
            Layout.fillWidth: true
            checkable: true
            checked: tabStack.currentIndex === 1
            enabled: false
            opacity: 0.4
            text: "VIDEO"
        }
        Skin.Button {
            Layout.fillWidth: true
            checkable: true
            checked: tabStack.currentIndex === 2
            text: "MASTER"

            onClicked: tabStack.currentIndex = 2
        }
    }
    StackLayout {
        id: tabStack

        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: 0

        Deo.AudioMixerPanel {
            accentColorA: root.accentColorA
            accentColorB: root.accentColorB
        }
        Item {
            // VIDEO: blocked on M12, no video engine exists.
        }
        Deo.MasterPanel {
        }
    }
    Deo.Crossfader {
        Layout.fillWidth: true
        Layout.preferredHeight: 26
    }
}
