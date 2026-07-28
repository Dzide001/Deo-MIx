import QtQuick 2.12
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// CUE, Play/Pause, SYNC. Rebuilt per the user's reference mockup
// (JogwheelDesignMockup.qml, which includes this row directly below the
// wheel) as a plain Item with the mockup's exact literal x coordinates
// (225 is the mockup's own reference canvas width, matching this row's
// real allocated width) rather than QtQuick Layouts.
//
// CUE and PLAY were previously Deck.CueButton/Deck.PlayButton (stock
// components) -- CUE's plain-text style already matched the mockup, but
// PlayButton overrides its contentItem with a triangle Shape icon instead
// of text, which the mockup doesn't show (just "PLAY" text). Rebuilt
// both directly on Skin.ControlButton here, keeping the real
// cue_default/play bindings, so both read as flat text buttons like the
// reference. SYNC stays Deo.SyncButton unchanged -- it already renders as
// plain text (no contentItem override) and has real leader-mode logic
// (color/text changes, press-and-hold) worth keeping intact.
Item {
    id: root

    required property string group
    required property color accentColor

    // Authored canvas: 225 wide (matching the mockup's own reference
    // width), 34 tall -- not the mockup's literal 25px slice. Confirmed
    // via a temporary debug marker that this row was rendering in the
    // right position at the right size; 25px was just too thin to read
    // (button text became illegible) at the deck's real allocated size.
    // 34 is still far more compact than the original 44px floor, just
    // enough taller to keep "Cue"/"Play"/"Sync" legible.
    height: 34
    width: 225

    readonly property bool trackLoaded: trackLoadedControl.value > 0

    readonly property real buttonWidth: 42
    readonly property real cueX: 43
    readonly property real playX: 92
    readonly property real syncX: 140

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: playControl

        group: root.group
        key: "play"
    }

    Skin.ControlButton {
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        height: root.height
        key: "cue_default"
        opacity: enabled ? 1.0 : 0.6
        text: "Cue"
        width: root.buttonWidth
        x: root.cueX
        y: 0
    }
    Skin.ControlButton {
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        height: root.height
        highlight: playControl.value > 0
        key: "play"
        opacity: enabled ? 1.0 : 0.6
        text: "Play"
        toggleable: root.trackLoaded
        width: root.buttonWidth
        x: root.playX
        y: 0
    }
    Deo.SyncButton {
        accentColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        height: root.height
        opacity: enabled ? 1.0 : 0.6
        width: root.buttonWidth
        x: root.syncX
        y: 0
    }
}
