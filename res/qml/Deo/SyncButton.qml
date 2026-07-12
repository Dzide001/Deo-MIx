import QtQuick 2.12
import ".." as Skin
import Mixxx 1.0 as Mixxx
import "../Theme"

// Same sync_enabled/sync_mode/sync_leader model as the stock SyncButton,
// but with an accent color that can be overridden per-deck. Persistent
// sync-lock (not one-shot) matches Mixxx's existing SyncControl semantics:
// enabling sync with no other leader present makes this deck the implicit
// leader rather than rejecting the press, which is the resolution for the
// M1 spec's "SYNC pressed with no leader available" edge case.
Skin.Button {
    id: root

    enum SyncMode {
        Off,
        Follower,
        ImplicitLeader,
        ExplicitLeader
    }

    required property string group
    required property color accentColor
    property alias mode: modeControl.value

    function toggleSync() {
        enabledControl.value = !enabledControl.value;
    }

    function toggleLeader() {
        leaderControl.value = !leaderControl.value;
    }

    activeColor: {
        switch (mode) {
        case SyncButton.SyncMode.ImplicitLeader:
            return Theme.yellow;
        case SyncButton.SyncMode.ExplicitLeader:
            return Theme.red;
        default:
            return root.accentColor;
        }
    }
    text: {
        switch (mode) {
        case SyncButton.SyncMode.ImplicitLeader:
        case SyncButton.SyncMode.ExplicitLeader:
            return "Leader";
        default:
            return "Sync";
        }
    }
    highlight: enabledControl.value
    onClicked: toggleSync()
    onPressAndHold: toggleLeader()

    Mixxx.ControlProxy {
        id: enabledControl

        group: root.group
        key: "sync_enabled"
    }
    Mixxx.ControlProxy {
        id: modeControl

        group: root.group
        key: "sync_mode"
    }
    Mixxx.ControlProxy {
        id: leaderControl

        group: root.group
        key: "sync_leader"
    }
}
