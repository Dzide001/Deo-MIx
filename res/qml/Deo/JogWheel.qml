import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls 2.12

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
//
// Visual state recoloring: the platter's border and center label recolor
// per state -- idle (root.accentColor, the deck's existing per-deck
// theme color), playing (green), cue point set (orange), scratching
// (red). A prior version also drew 4 small corner "rim marker" tick
// rectangles flanking the platter below -- removed per explicit user
// decision (not part of the reference jog wheel design, which uses a
// single ring of segments wrapping the platter's own edge instead of
// separate flanking marks; that full ring is a bigger visual feature not
// yet built here). None of the scratch/jog/bend control logic below
// changed from the prior version.
Item {
    id: root

    required property string group
    required property color accentColor
    property bool vinylMode: true

    readonly property bool trackLoaded: trackLoadedControl.value > 0
    readonly property bool isPlaying: playControl.value > 0
    readonly property bool touched: wheelArea.pressed
    readonly property bool scratching: touched && root.vinylMode && root.isPlaying
    readonly property bool cueSet: cueIndicatorControl.value > 0

    // Four visual states, most-specific-wins: scratching over cue-set
    // over playing over idle. Idle deliberately uses the deck's existing
    // per-deck accentColor (Deck A/B already have distinct theme colors
    // elsewhere in the skin) rather than a hardcoded color, so the ring
    // stays consistent with the rest of that deck's theming; the other
    // three are fixed semantic status colors, matching the reference
    // mockup's state panel (green/orange/red) regardless of deck theme.
    readonly property color stateColor: {
        if (!root.trackLoaded)
            return "#555555";
        if (root.scratching)
            return "#E5484D";
        if (root.cueSet)
            return "#F2A33C";
        if (root.isPlaying)
            return "#3FB950";
        return root.accentColor;
    }

    // Tuned by feel, not derived from a physical model.
    readonly property real scratchSensitivity: 4000
    readonly property real bendSensitivity: 0.5
    readonly property real pausedScrubSensitivity: 0.5

    implicitWidth: 160
    implicitHeight: 160

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
    Mixxx.ControlProxy {
        id: cueIndicatorControl

        group: root.group
        key: "cue_indicator"
    }
    // Beat-synced pulse: fraction of progress through the current beat
    // (0 at the beat, approaching 1 just before the next one). Brightens
    // the beat ring right on each beat and fades toward the next, rather
    // than a fixed-rate animation unrelated to the actual track.
    Mixxx.ControlProxy {
        id: beatDistanceControl

        group: root.group
        key: "beat_distance"
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

    // Beat ring: a continuous ring of small radial tick segments wrapping
    // the platter's outer edge, matching the user's reference design
    // ("BEAT RING (pulses)") -- replaces the previous 4 flanking corner
    // marks entirely. Positioned by polar math (angle -> x/y) since QML
    // has no built-in circular layout; x/y is each segment's own
    // center-adjusted position, radius measured from this ring's own
    // center. Brightens together on each beat via beatRing.pulse, same
    // formula the old corner marks used.
    Item {
        id: beatRing

        // +18 (radius +9 past platter's own edge: a 6px real gap from
        // the disc's edge to the segment ring's own inner edge, plus the
        // 6px-long segment itself) -- not the previous +12, which still
        // left segments effectively touching the disc's border stroke.
        anchors.centerIn: parent
        width: platter.width + 18
        height: width

        readonly property real radius: width / 2
        readonly property real pulse: root.isPlaying ? (0.4 + 0.6 * (1 - beatDistanceControl.value)) : 0.5
        readonly property int segmentCount: 48
        readonly property real segmentLength: 6
        readonly property real segmentThickness: 2.2

        Repeater {
            model: beatRing.segmentCount

            Rectangle {
                id: segment

                required property int index

                readonly property real angleDeg: segment.index * (360 / beatRing.segmentCount)
                readonly property real angleRad: segment.angleDeg * Math.PI / 180

                x: beatRing.radius + beatRing.radius * Math.sin(segment.angleRad) - width / 2
                y: beatRing.radius - beatRing.radius * Math.cos(segment.angleRad) - height / 2
                width: beatRing.segmentThickness
                height: beatRing.segmentLength
                radius: width / 2
                color: root.stateColor
                opacity: root.trackLoaded ? beatRing.pulse : 0.2
                rotation: segment.angleDeg

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }
            }
        }
    }
    // Sub-beat markers: a sparser, static ring of small dots just outside
    // the beat ring ("SUB-BEAT MARKERS" in the reference), same polar
    // positioning technique as beatRing above.
    Item {
        id: subBeatRing

        // +39 over platter (not chained off beatRing.width, to avoid
        // compounding rounding): beatRing's segments are centered at
        // platter radius +9 with half-length 3, so their outer edge sits
        // at platter radius +12; a further 6px real gap plus the dot's
        // own 1.5px half-size lands this ring's center at platter radius
        // +19.5, i.e. width platter.width+39.
        anchors.centerIn: parent
        width: platter.width + 39
        height: width

        readonly property real radius: width / 2
        readonly property int dotCount: 16
        readonly property real dotSize: 3

        Repeater {
            model: subBeatRing.dotCount

            Rectangle {
                id: dot

                required property int index

                readonly property real angleRad: dot.index * (360 / subBeatRing.dotCount) * Math.PI / 180

                x: subBeatRing.radius + subBeatRing.radius * Math.sin(dot.angleRad) - width / 2
                y: subBeatRing.radius - subBeatRing.radius * Math.cos(dot.angleRad) - height / 2
                width: subBeatRing.dotSize
                height: subBeatRing.dotSize
                radius: width / 2
                color: root.stateColor
                opacity: root.trackLoaded ? 0.5 : 0.15

                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }
            }
        }
    }

    Rectangle {
        id: platter

        // -46, not -34 -- 34 gave the ring system ~17px of outward room,
        // but platter's own border stroke is drawn centered ON its
        // boundary (half bleeds outward past the nominal radius), so a
        // 3px gap to the segment ring left only ~1px of real clearance
        // past the border's own outer edge -- too thin to read as a
        // separate ring rather than "touching the disc" at a glance,
        // let alone after this whole canvas gets scaled down further to
        // fit the deck's real allocated size. 46 leaves ~23px, enough
        // for genuinely visible ~6px gaps at every boundary below.
        anchors.centerIn: parent
        width: Math.min(root.width, root.height) - 46
        height: width
        radius: width / 2
        color: "#2A2A2A"
        opacity: root.trackLoaded ? 1.0 : 0.35
        border.width: root.touched ? 4 : 2
        border.color: root.stateColor

        Behavior on border.color {
            ColorAnimation {
                duration: 150
            }
        }
        Behavior on border.width {
            NumberAnimation {
                duration: 100
            }
        }

        // Center brand mark, recolored per state.
        Label {
            anchors.centerIn: parent
            color: root.stateColor
            font.bold: true
            font.pixelSize: platter.width * 0.16
            text: "Deo"

            Behavior on color {
                ColorAnimation {
                    duration: 150
                }
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: platter.width * 0.42
            height: width
            radius: width / 2
            color: "transparent"
            border.width: 1
            border.color: root.stateColor
            opacity: 0.6
        }

        // Playhead position indicator: a tick that rotates with playback
        // position, same math as Mixxx/Controls/Spinny.qml's indicator --
        // unchanged from the prior version, just recolored per state.
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
                color: root.trackLoaded ? root.stateColor : "#555555"

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
