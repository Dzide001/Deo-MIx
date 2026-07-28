import QtQuick 2.12
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// Milestone 2 loop section, rebuilt per the user's reference mockup
// (CustomLoopSectionDesignMockup.qml) as a plain Item with the mockup's
// exact literal coordinates (it was drawn at DB lower half's real
// allocated size, 139x108, so these numbers are direct pixel targets, not
// a proportion to rederive) rather than QtQuick Layouts.
//
// IN/OUT (manual loop points, always available) coexist with a
// beatloop-size readout, matching stock Deck/Loop.qml's model (confirmed)
// rather than a mutually-exclusive one. The readout's </> step buttons
// halve/double (confirmed, not loop_move): when a loop is active they
// resize it live; when idle they step which size the readout button will
// activate next. Tapping the readout activates a regular beatloop;
// holding it activates a rolling loop instead (beatlooproll_*_activate is
// a momentary press/release control at the engine level, so hold = on,
// release = off, same pattern as CUE in M1).
//
// The mockup's bottom row only drew IN/OUT -- the third Rec/Exit button
// (loop recall/exit, a real wired control) is kept per explicit user
// decision, fit into the same row's span by splitting it three ways
// instead of two.
Item {
    id: root

    required property string group
    required property color accentColor

    // Fixed authored canvas, matching CustomLoopSectionDesignMockup.qml
    // exactly -- DeckPanel.qml scales this to fit the real allocated box
    // (a Scale transform, not anchors.fill) rather than assuming the real
    // box already equals 139x108.
    height: 108
    width: 139

    readonly property bool trackLoaded: trackLoadedControl.value > 0
    readonly property bool loopActive: loopEnabledControl.value > 0
    property var beatSizes: [1 / 32, 1 / 16, 1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512]
    property int selectedIndex: 8 // 8 beats

    // Geometry, taken directly from CustomLoopSectionDesignMockup.qml.
    readonly property real labelX: 1
    readonly property real labelWidth: 28
    readonly property real row1Y: 26
    readonly property real row2Y: 57
    readonly property real rowHeight: 25
    readonly property real lessX: 34
    readonly property real lessWidth: 21
    readonly property real readoutX: 61
    readonly property real readoutWidth: 35
    readonly property real moreX: 102
    readonly property real moreWidth: 22
    // Bottom row split three ways (In/Out/Rec) across the same span the
    // mockup's two-button row occupied (x34-124).
    readonly property real bottomRowX: 34
    readonly property real bottomRowWidth: 90
    readonly property real bottomButtonWidth: (root.bottomRowWidth - 2 * 3) / 3
    readonly property real inX: root.bottomRowX
    readonly property real outX: root.bottomRowX + root.bottomButtonWidth + 3
    readonly property real recX: root.bottomRowX + 2 * (root.bottomButtonWidth + 3)

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: loopEnabledControl

        group: root.group
        key: "loop_enabled"
    }
    Mixxx.ControlProxy {
        id: beatloopSizeControl

        group: root.group
        key: "beatloop_size"
    }
    Mixxx.ControlProxy {
        id: loopHalve

        group: root.group
        key: "loop_halve"
    }
    Mixxx.ControlProxy {
        id: loopDouble

        group: root.group
        key: "loop_double"
    }

    // Keep the readout tracking the live loop size while a loop is active.
    Connections {
        function onValueChanged() {
            if (root.loopActive) {
                const idx = root.beatSizes.indexOf(beatloopSizeControl.value);
                if (idx >= 0) {
                    root.selectedIndex = idx;
                }
            }
        }

        target: beatloopSizeControl
    }

    function formatBeatSize(value) {
        if (value < 1) {
            return "1/" + Math.round(1 / value);
        }
        return Math.round(value).toString();
    }

    Mixxx.ControlProxy {
        id: quantizeControl

        group: root.group
        key: "quantize"
    }
    // Loop settings menu, matching the reference's dot-triggered popup.
    // Only "Quantize" is wired for real -- it's a real per-channel CO.
    // "Smart Loop" (adaptive auto-sizing) and "Loop Direction"
    // (continuous forward/backward/ping-pong) have no equivalent
    // anywhere in Mixxx's engine; "Roll" has no persistent/global mode
    // toggle, only the existing per-size momentary roll control this
    // section's readout button already uses via press-and-hold. Logged
    // as deferred follow-ups in master_decision_list.md rather than
    // building fake menu items with nothing real to bind to.
    Menu {
        id: loopSettingsMenu

        MenuItem {
            checkable: true
            checked: quantizeControl.value > 0
            text: "Quantize"

            onTriggered: quantizeControl.value = checked ? 1 : 0
        }
    }
    // Rotated label on the left edge, matching the reference layout, with
    // a small settings dot near the bottom opening loopSettingsMenu.
    Item {
        height: root.height
        width: root.labelWidth
        x: root.labelX
        y: 0

        Label {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -6
            color: Theme.deckLoopLabelColor
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 10
            rotation: -90
            text: "LOOP"
        }
        Rectangle {
            id: settingsDot

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 4
            anchors.horizontalCenter: parent.horizontalCenter
            color: quantizeControl.value > 0 ? root.accentColor : Theme.deckLoopLabelColor
            height: 8
            radius: 4
            width: 8

            TapHandler {
                onTapped: loopSettingsMenu.popup(settingsDot)
            }
        }
    }
    // Row 1: size stepper (</readout/>).
    Skin.Button {
        activeColor: root.accentColor
        enabled: root.trackLoaded
        height: root.rowHeight
        opacity: enabled ? 1.0 : 0.4
        text: "<"
        width: root.lessWidth
        x: root.lessX
        y: root.row1Y

        onClicked: {
            if (root.loopActive) {
                loopHalve.trigger();
            } else {
                root.selectedIndex = Math.max(0, root.selectedIndex - 1);
            }
        }
    }
    Skin.Button {
        id: activateButton

        property bool rolling: false

        activeColor: root.accentColor
        enabled: root.trackLoaded
        highlight: root.loopActive
        height: root.rowHeight
        opacity: enabled ? 1.0 : 0.4
        text: root.formatBeatSize(root.beatSizes[root.selectedIndex])
        width: root.readoutWidth
        x: root.readoutX
        y: root.row1Y

        onClicked: {
            if (!root.loopActive && !rolling) {
                activateControl.trigger();
            }
        }
        onPressAndHold: {
            if (!root.loopActive) {
                rolling = true;
                rollControl.value = 1;
            }
        }
        onReleased: {
            if (rolling) {
                rollControl.value = 0;
                rolling = false;
            }
        }

        Mixxx.ControlProxy {
            id: activateControl

            group: root.group
            key: `beatloop_${root.beatSizes[root.selectedIndex]}_activate`
        }
        Mixxx.ControlProxy {
            id: rollControl

            group: root.group
            key: `beatlooproll_${root.beatSizes[root.selectedIndex]}_activate`
        }
    }
    Skin.Button {
        activeColor: root.accentColor
        enabled: root.trackLoaded
        height: root.rowHeight
        opacity: enabled ? 1.0 : 0.4
        text: ">"
        width: root.moreWidth
        x: root.moreX
        y: root.row1Y

        onClicked: {
            if (root.loopActive) {
                loopDouble.trigger();
            } else {
                root.selectedIndex = Math.min(root.beatSizes.length - 1, root.selectedIndex + 1);
            }
        }
    }
    // Row 2: loop points (In/Out/Rec-Exit).
    Skin.ControlButton {
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        height: root.rowHeight
        key: "loop_in"
        opacity: enabled ? 1.0 : 0.4
        text: "In"
        width: root.bottomButtonWidth
        x: root.inX
        y: root.row2Y
    }
    Skin.ControlButton {
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        height: root.rowHeight
        key: "loop_out"
        opacity: enabled ? 1.0 : 0.4
        text: "Out"
        width: root.bottomButtonWidth
        x: root.outX
        y: root.row2Y
    }
    Skin.ControlButton {
        // "Rec", not "Recall" -- Button.qml's Label has no elide/wrap (a
        // stock component, not safe to modify here), so the full word
        // wouldn't reliably fit this component's fixed 10px bold
        // uppercase font.
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        height: root.rowHeight
        key: root.loopActive ? "loop_enabled" : "reloop_toggle"
        opacity: enabled ? 1.0 : 0.4
        text: root.loopActive ? "Exit" : "Rec"
        toggleable: root.loopActive
        width: root.bottomButtonWidth
        x: root.recX
        y: root.row2Y
    }
}
