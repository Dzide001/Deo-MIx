import Mixxx 1.0 as Mixxx
import QtQuick 2.12

// Milestone 1 jog wheel: VINYL and SLIP are independent boolean axes.
//
// VINYL (root.vinylMode) controls what touching/turning the platter does:
//   ON  (scratch mode): touch pauses/engages scratch, turning scratches
//       via scratch_position_enable + scratch_position.
//   OFF (bend mode): touching does nothing, turning is a temporary pitch
//       bend via the "jog" accumulator CO (springs back on its own).
//
// Regardless of VINYL, if the deck is paused, wheel motion always scrubs
// through the track via "jog" and never touches scratch_position_enable
// (see milestone_1_deck_transport_spec.md "Jog wheel behavior").
//
// SLIP (slip_enabled) is wired directly to the engine; slip catch-up on
// release of a scratch is stock Mixxx EngineBuffer/SlipModeControl
// behavior, not reimplemented here.
Item {
    id: root

    required property string group
    required property color accentColor
    property bool vinylMode: true

    readonly property bool trackLoaded: trackLoadedControl.value > 0
    readonly property bool isPlaying: playControl.value > 0
    readonly property bool touched: wheelArea.pressed

    // Tuned by feel, not derived from a physical model.
    readonly property real scratchSensitivity: 4000
    readonly property real bendSensitivity: 0.5
    readonly property real pausedScrubSensitivity: 0.5

    implicitWidth: 180
    implicitHeight: 180

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
    Mixxx.ControlProxy {
        id: playPositionControl

        group: root.group
        key: "playposition"
    }
    Mixxx.ControlProxy {
        id: samplesControl

        group: root.group
        key: "track_samples"
    }
    Mixxx.ControlProxy {
        id: sampleRateControl

        group: root.group
        key: "track_samplerate"
    }
    Mixxx.ControlProxy {
        id: scratchEnableControl

        group: root.group
        key: "scratch_position_enable"
    }
    Mixxx.ControlProxy {
        id: scratchPositionControl

        group: root.group
        key: "scratch_position"
    }
    Mixxx.ControlProxy {
        id: jogControl

        group: root.group
        key: "jog"
    }

    // Edge case: rapid VINYL toggle while actively scratching must cleanly
    // disable scratch rather than leave it stuck on.
    onVinylModeChanged: {
        if (!root.vinylMode && scratchEnableControl.value) {
            scratchEnableControl.value = 0;
        }
    }
    // Edge case: track unloads/ejects mid-scratch.
    onTrackLoadedChanged: {
        if (!root.trackLoaded && scratchEnableControl.value) {
            scratchEnableControl.value = 0;
        }
    }

    Rectangle {
        id: platter

        anchors.centerIn: parent
        width: Math.min(root.width, root.height)
        height: width
        radius: width / 2
        color: "#2A2A2A"
        opacity: root.trackLoaded ? 1.0 : 0.35
        border.width: root.touched ? 4 : 1
        border.color: root.touched ? root.accentColor : "#454545"

        Behavior on border.color {
            ColorAnimation {
                duration: 100
            }
        }
        Behavior on border.width {
            NumberAnimation {
                duration: 100
            }
        }

        // Idle visual: a tick that rotates with playback position, same
        // math as Mixxx/Controls/Spinny.qml's indicator.
        Item {
            id: rotator

            anchors.fill: parent

            readonly property real rpm: 33.33
            readonly property real rps: Math.PI * rpm / 60.0
            readonly property real totalFrames: samplesControl.value / 2
            readonly property real positionSeconds: (sampleRateControl.value > 0) ? playPositionControl.value * totalFrames / sampleRateControl.value : 0
            readonly property real rotationFactor: (rps / Math.PI) * positionSeconds % 1

            transform: Rotation {
                origin.x: rotator.width / 2
                origin.y: rotator.height / 2
                angle: 360 * rotator.rotationFactor
            }

            Rectangle {
                width: 3
                height: parent.height / 2 - 12
                radius: 1.5
                color: root.trackLoaded ? "#CFCFCF" : "#555555"

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    top: parent.top
                    topMargin: 8
                }
            }
        }
    }

    MouseArea {
        id: wheelArea

        property real lastAngle: 0

        function angleAt(x, y) {
            return Math.atan2(y - height / 2, x - width / 2);
        }

        anchors.fill: platter
        enabled: root.trackLoaded
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor

        onPressed: mouse => {
            lastAngle = angleAt(mouse.x, mouse.y);
            if (root.vinylMode && root.isPlaying) {
                scratchPositionControl.value = 0.0;
                scratchEnableControl.value = 1;
            }
        }
        onPositionChanged: mouse => {
            const currentAngle = angleAt(mouse.x, mouse.y);
            let delta = currentAngle - lastAngle;
            while (delta > Math.PI)
                delta -= 2 * Math.PI;
            while (delta < -Math.PI)
                delta += 2 * Math.PI;
            lastAngle = currentAngle;

            if (!root.isPlaying) {
                // Paused: always scrub, regardless of VINYL state.
                jogControl.value += delta * root.pausedScrubSensitivity;
            } else if (root.vinylMode) {
                scratchPositionControl.value += delta * root.scratchSensitivity;
            } else {
                jogControl.value += delta * root.bendSensitivity;
            }
        }
        onReleased: {
            if (scratchEnableControl.value) {
                scratchEnableControl.value = 0;
            }
        }
    }
}
