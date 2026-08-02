import QtQuick 2.12

// M4/pad-bank-switching: the Custom bank's curated action list -- real,
// existing Mixxx ControlObjects only (pulled from
// src/controllers/controlpickermenu.cpp, Mixxx's own curated MIDI-mapping
// control picker), never invented plausible-sounding ones. This is
// deliberately a pick-from-a-list, not a live "click any control to learn
// it" system -- see the class doc comment in CustomPad.qml for why.
//
// `group: ""` means "relative to whichever deck's Custom bank this pad
// lives on" (the common case); a non-empty `group` is an explicit
// override for the few actions that aren't per-deck (e.g. "Internal Sync
// Leader" below always targets the app's internal clock, regardless of
// which deck's pad triggers it).
//
// `toggleable` maps directly to Skin.ControlButton's own property: true
// for a real on/off state CO (press to flip, stays until pressed again),
// false for a momentary pushbutton CO -- which covers BOTH "hold to
// sustain" actions (Reverse) and "one-shot pulse" actions (Cue Set): both
// are simple press=1/release=0 pushbuttons at the QML binding layer, they
// only differ in whether the underlying engine control cares about the
// duration (hold) or just the rising edge (pulse) -- no QML-side
// distinction needed.
ListModel {
    ListElement {
        label: "Reverse"
        key: "reverse"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Reverse Roll (Censor)"
        key: "reverseroll"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Backspin"
        key: "backspin_activate"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Preview Cue"
        key: "cue_preview"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Slip Mode"
        key: "slip_enabled"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Keylock"
        key: "keylock"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Quantize Mode"
        key: "quantize"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Headphone Listen (PFL)"
        key: "pfl"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Sync Lock"
        key: "sync_enabled"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Beat Sync One-Shot"
        key: "beatsync"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Cue"
        key: "cue_default"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Set Cue"
        key: "cue_set"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Go-To Cue And Play"
        key: "cue_gotoandplay"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Go-To Cue And Stop"
        key: "cue_gotoandstop"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Stutter Cue"
        key: "play_stutter"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Loop In"
        key: "loop_in"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Loop Out"
        key: "loop_out"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Reloop / Exit Toggle"
        key: "reloop_toggle"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Loop Halve"
        key: "loop_halve"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Loop Double"
        key: "loop_double"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Beatloop 4 Toggle"
        key: "beatloop_4_toggle"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Beatloop 8 Toggle"
        key: "beatloop_8_toggle"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Beatloop 16 Toggle"
        key: "beatloop_16_toggle"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Beatjump Forward"
        key: "beatjump_forward"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Beatjump Backward"
        key: "beatjump_backward"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "BPM Tap"
        key: "bpm_tap"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Eject"
        key: "eject"
        toggleable: false
        group: ""
    }
    ListElement {
        label: "Play / Pause"
        key: "play"
        toggleable: true
        group: ""
    }
    ListElement {
        label: "Sync Leader"
        key: "sync_leader"
        toggleable: true
        group: ""
    }
    // Real example of the explicit-group-override mechanic: this one
    // always targets the app's internal clock, regardless of which deck's
    // Custom bank the pad assigned to it lives on.
    ListElement {
        label: "Internal Sync Leader"
        key: "sync_leader"
        toggleable: true
        group: "[InternalClock]"
    }
}
