import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// Milestone 2 loop section: IN/OUT (manual loop points, always available)
// coexist with a beatloop-size readout, matching stock Deck/Loop.qml's
// model (confirmed) rather than a mutually-exclusive one. The readout's
// </> step buttons halve/double (confirmed, not loop_move): when a loop
// is active they resize it live; when idle they step which size the
// readout button will activate next. Tapping the readout activates a
// regular beatloop; holding it activates a rolling loop instead
// (beatlooproll_*_activate is a momentary press/release control at the
// engine level, so hold = on, release = off, same pattern as CUE in M1).
RowLayout {
    id: root

    required property string group
    required property color accentColor

    readonly property bool trackLoaded: trackLoadedControl.value > 0
    readonly property bool loopActive: loopEnabledControl.value > 0
    property var beatSizes: [1 / 32, 1 / 16, 1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512]
    property int selectedIndex: 8 // 8 beats

    spacing: 4

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

    // Rotated label on the left edge, matching the reference layout.
    Item {
        Layout.fillHeight: true
        Layout.preferredWidth: 16

        Label {
            anchors.centerIn: parent
            color: Theme.deckLoopLabelColor
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 10
            rotation: -90
            text: "LOOP"
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            enabled: root.trackLoaded
            opacity: enabled ? 1.0 : 0.4
            spacing: 4

            Skin.Button {
                Layout.preferredWidth: 22
                activeColor: root.accentColor
                implicitHeight: 22
                text: "<"

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

                Layout.fillWidth: true
                activeColor: root.accentColor
                highlight: root.loopActive
                implicitHeight: 22
                text: root.formatBeatSize(root.beatSizes[root.selectedIndex])

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
                Layout.preferredWidth: 22
                activeColor: root.accentColor
                implicitHeight: 22
                text: ">"

                onClicked: {
                    if (root.loopActive) {
                        loopDouble.trigger();
                    } else {
                        root.selectedIndex = Math.min(root.beatSizes.length - 1, root.selectedIndex + 1);
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Skin.ControlButton {
                Layout.fillWidth: true
                activeColor: root.accentColor
                enabled: root.trackLoaded
                group: root.group
                implicitHeight: 24
                key: "loop_in"
                opacity: enabled ? 1.0 : 0.4
                text: "In"
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                activeColor: root.accentColor
                enabled: root.trackLoaded
                group: root.group
                implicitHeight: 24
                key: "loop_out"
                opacity: enabled ? 1.0 : 0.4
                text: "Out"
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                activeColor: root.accentColor
                enabled: root.trackLoaded
                group: root.group
                implicitHeight: 24
                key: root.loopActive ? "loop_enabled" : "reloop_toggle"
                opacity: enabled ? 1.0 : 0.4
                text: root.loopActive ? "Exit" : "Recall"
                toggleable: root.loopActive
            }
        }
    }
}
