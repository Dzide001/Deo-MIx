import QtQuick 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin

// Milestone 3 FX rack: Backspin (fake row, see BackspinSlot.qml) + two real
// effect slots (Reverb/WahWah in the reference, but any effect is
// selectable via the dropdown same as stock Mixxx). unitNumber permanently
// routes to this deck's channel on load, so each deck's rack only ever
// processes its own audio.
ColumnLayout {
    id: root

    required property string group
    required property int unitNumber
    required property color accentColor

    readonly property bool trackLoaded: trackLoadedControl.value > 0

    spacing: 4
    enabled: root.trackLoaded
    opacity: enabled ? 1.0 : 0.4

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    // Pin this unit to this deck's channel permanently; M3 is a fixed
    // per-deck rack, not a flexible any-unit-to-any-channel router.
    Mixxx.ControlProxy {
        id: routingControl

        group: "[EffectRack1_EffectUnit" + root.unitNumber + "]"
        key: "group_" + root.group + "_enable"

        onInitializedChanged: {
            value = 1;
        }
    }

    Deo.BackspinSlot {
        Layout.fillWidth: true
        accentColor: root.accentColor
        group: root.group
    }
    Skin.EffectSlot {
        Layout.fillWidth: true
        accentColor: root.accentColor
        effectNumber: 1
        unitNumber: root.unitNumber
    }
    Skin.EffectSlot {
        Layout.fillWidth: true
        accentColor: root.accentColor
        effectNumber: 2
        unitNumber: root.unitNumber
    }
}
