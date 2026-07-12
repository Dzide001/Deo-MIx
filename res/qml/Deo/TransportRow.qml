import QtQuick 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo
import "../Deck" as Deck

// CUE, Play/Pause, SYNC. CUE and Play reuse the stock Deck button
// components as-is (their cue_default/play bindings already match the M1
// spec); SYNC is Deo.SyncButton so the accent color can be overridden
// without losing the leader-mode color states.
RowLayout {
    id: root

    required property string group
    required property color accentColor

    readonly property bool trackLoaded: trackLoadedControl.value > 0

    spacing: 8

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }

    Deck.CueButton {
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        opacity: enabled ? 1.0 : 0.4
    }
    Deck.PlayButton {
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        opacity: enabled ? 1.0 : 0.4
    }
    Deo.SyncButton {
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        accentColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        opacity: enabled ? 1.0 : 0.4
    }
}
