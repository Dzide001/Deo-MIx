import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import Mixxx 1.0 as Mixxx
import "." as Deo
import "../Theme"

// Upper Deck: the deck's track-identity + at-a-glance readout strip,
// replacing the old plain "DECK A / No track loaded" header row (and
// absorbing the old standalone overview-waveform row -- it now lives in
// UDDL below). Structure, per the user's spec:
//
//   UpperDeck (~25-30% of the deck panel's height)
//   +-- UDT (~60% height)
//   |   +-- UDTL (78% width): deck label, album art, title, artist
//   |   +-- UDTR (22% width): BPM (left) | KEY (right)
//   +-- UDD (~40% height)
//       +-- UDDL (78% width): whole-track overview waveform
//       +-- UDDR (22% width): ELAPSED (left) | REMAIN (right)
//
// UDDL/UDDR's split isn't independently specified in the user's spec;
// mirrored to match UDTL/UDTR's ratio until told otherwise.
//
// UDT/UDD, UDTL/UDTR, and UDDL/UDDR are plain preferredHeight/preferredWidth
// *weights* paired with fillHeight/fillWidth, not `parent.height * 0.6`
// -- matching the established pattern in DeckPanel.qml/main.qml (a
// direct-parent-width/height read from a child is a circular binding
// Qt Quick Layouts can't reliably resolve; a weight sidesteps that and
// still always sums to exactly the available space).
Item {
    id: root

    required property string group
    required property string label
    required property color accentColor
    property bool mirrored: false

    property var deckPlayer: Mixxx.PlayerManager.getPlayer(root.group)
    readonly property var currentTrack: deckPlayer?.currentTrack
    readonly property bool trackLoaded: deckPlayer?.isLoaded ?? false

    function formatTime(seconds) {
        if (!isFinite(seconds) || seconds < 0) {
            return "00:00";
        }
        const m = Math.floor(seconds / 60);
        const s = Math.floor(seconds % 60);
        return m.toString().padStart(2, '0') + ":" + s.toString().padStart(2, '0');
    }

    implicitHeight: column.implicitHeight

    Mixxx.ControlProxy {
        id: bpmControl

        group: root.group
        key: "bpm"
    }
    Mixxx.ControlProxy {
        id: keylockControl

        group: root.group
        key: "keylock"
    }
    // Total semitone offset from the track's file key (KeyControl's
    // "pitch" CO, -6..+6, out-of-bounds allowed). Written directly by the
    // -/+ transpose buttons and the target-key picker -- this fork's
    // KeyControl has no pitch_up/pitch_down button COs to reuse.
    Mixxx.ControlProxy {
        id: pitchControl

        group: root.group
        key: "pitch"
    }
    // Effective (post-transpose) key as a ChromaticKey number, kept
    // up to date by KeyControl as pitch/rate change. 1-12 = C..B major,
    // 13-24 = C..B minor, both chromatically ascending (src/proto/keys.proto).
    Mixxx.ControlProxy {
        id: engineKeyControl

        group: root.group
        key: "key"
    }

    // Display names indexed by ChromaticKey value (index 0 = INVALID).
    // Deliberately a fixed traditional-notation table rather than the
    // config-dependent KeyUtils notation, matching the mockup ("Cm").
    readonly property var keyNames: ["--",
        "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
        "Cm", "C#m", "Dm", "Ebm", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "Bbm", "Bm"]

    readonly property int effectiveKey: {
        const k = Math.round(engineKeyControl.value);
        return (k >= 1 && k <= 24) ? k : 0;
    }
    readonly property string effectiveKeyText: keyNames[effectiveKey]

    // Transpose the track so its effective key becomes targetKey,
    // taking the shortest path (delta wrapped to -5..+6 semitones).
    function transposeToKey(targetKey) {
        if (effectiveKey === 0 || targetKey === 0) {
            return;
        }
        const currentPc = (effectiveKey - 1) % 12;
        const targetPc = (targetKey - 1) % 12;
        let delta = (targetPc - currentPc) % 12;
        if (delta > 6) {
            delta -= 12;
        } else if (delta < -5) {
            delta += 12;
        }
        pitchControl.value = pitchControl.value + delta;
    }
    Mixxx.ControlProxy {
        id: durationControl

        group: root.group
        key: "duration"
    }
    Mixxx.ControlProxy {
        id: playPositionControl

        group: root.group
        key: "playposition"
    }

    ColumnLayout {
        id: column

        anchors.fill: parent
        spacing: 4

        LayoutMirroring.enabled: root.mirrored
        LayoutMirroring.childrenInherit: true

        // UDT
        RowLayout {
            id: udt

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            spacing: 8

            // UDTL
            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 78
                spacing: 6

                // Fixed container the art fills via anchors, not a
                // self-referential `Layout.preferredWidth: height` on the
                // Image itself -- a Layout-attached-property binding that
                // reads the same item's own height back is evaluation-
                // order-fragile (unlike a plain `height: width` binding on
                // a non-Layout-managed Item, which is a normal, safe QML
                // idiom): if the Layout system reads preferredWidth before
                // fillHeight has resolved a real height, it can settle on
                // a degenerate size, and a zero/negative-sized
                // ShaderEffectSource behind OpacityMask below has been
                // observed to fall back to covering far more area than
                // intended instead of just rendering empty.
                Item {
                    id: coverArtContainer

                    Layout.fillHeight: true
                    // 50% wider than the original 25 weight.
                    Layout.preferredWidth: 38

                    Image {
                        id: coverArt

                        anchors.fill: parent
                        asynchronous: true
                        source: root.currentTrack?.coverArtUrl ?? ""
                        visible: false
                    }
                    Rectangle {
                        id: coverArtPlaceholder

                        anchors.fill: parent
                        color: Theme.deckEmptyCoverArt
                        radius: 4
                        visible: !root.trackLoaded
                    }
                    OpacityMask {
                        anchors.fill: parent
                        maskSource: coverArtPlaceholder
                        source: coverArt
                        visible: root.trackLoaded
                    }
                }

                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    // Paired with coverArtContainer's 38 above (was 25/75).
                    Layout.preferredWidth: 62
                    spacing: 1

                    RowLayout {
                        spacing: 4

                        Rectangle {
                            width: 8
                            height: 8
                            radius: 4
                            color: root.accentColor
                        }
                        Label {
                            color: Theme.deckTextSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: 10
                            text: root.label
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        color: Theme.deckTextBright
                        elide: Text.ElideRight
                        font.bold: true
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.textFontPixelSize
                        text: root.trackLoaded ? (root.currentTrack?.title ?? "") : "No track loaded — drop a file here"
                    }
                    Label {
                        Layout.fillWidth: true
                        color: Theme.deckTextSecondary
                        elide: Text.ElideRight
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        text: root.trackLoaded ? (root.currentTrack?.artist ?? "") : ""
                        visible: text.length > 0
                    }
                }
            }
            // UDTR -- per DeckTimeKeyMockup.qml (110x99): BPM keeps the
            // left ~42% at full height; the right ~58% splits vertically
            // into the big KEY readout with the keylock toggle (top,
            // weight 35) and a key-controls row (bottom, weight 21) with
            // semitone -/+ transpose and a target-key picker.
            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 22
                spacing: 4

                InfoCell {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 42
                    label: "BPM"
                    value: root.trackLoaded && bpmControl.value > 0 ? bpmControl.value.toFixed(1) : "--"
                }
                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.preferredWidth: 58
                    spacing: 2

                    // Big KEY readout + keylock toggle. Shows the
                    // EFFECTIVE key (post-transpose, from the "key" CO),
                    // not the file key -- the whole point of the controls
                    // below is that these can differ.
                    RowLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 35
                        spacing: 2

                        ColumnLayout {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            spacing: 0

                            Label {
                                color: Theme.deckTextSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: 9
                                text: "KEY"
                            }
                            Label {
                                Layout.fillWidth: true
                                color: Theme.deckTextBright
                                elide: Text.ElideRight
                                font.bold: true
                                font.family: Theme.fontFamily
                                font.pixelSize: 17
                                text: root.trackLoaded ? root.effectiveKeyText : "--"
                            }
                        }
                        // Keylock: lock icon, accent-lit when engaged.
                        // All colors derived from the deck's own accent
                        // (per the user: mockup colors were arrangement
                        // guidance only; real colors follow the deck's
                        // theme).
                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter
                            border.color: keylockControl.value ? root.accentColor : "transparent"
                            border.width: 1
                            color: keylockControl.value ? Qt.darker(root.accentColor, 1.5) : Qt.darker(root.accentColor, 3.2)
                            height: 20
                            radius: 3
                            width: 20

                            Image {
                                anchors.centerIn: parent
                                fillMode: Image.PreserveAspectFit
                                height: 13
                                opacity: keylockControl.value ? 1.0 : 0.55
                                source: keylockControl.value ? "lock-closed.svg" : "lock-open.svg"
                                width: 12
                            }
                            MouseArea {
                                anchors.fill: parent

                                onClicked: keylockControl.value = keylockControl.value ? 0 : 1
                            }
                        }
                    }
                    // Key-controls row: semitone -/+ and target-key picker.
                    RowLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.preferredHeight: 21
                        spacing: 2

                        KeyMiniCell {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.preferredWidth: 50

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 3

                                KeyMiniButton {
                                    enabled: root.trackLoaded
                                    text: "−"

                                    onClicked: pitchControl.value = pitchControl.value - 1
                                }
                                KeyMiniButton {
                                    enabled: root.trackLoaded
                                    text: "+"

                                    onClicked: pitchControl.value = pitchControl.value + 1
                                }
                            }
                        }
                        KeyMiniCell {
                            id: keyPickerCell

                            Layout.fillHeight: true
                            Layout.fillWidth: true
                            Layout.preferredWidth: 50

                            Label {
                                anchors.centerIn: parent
                                color: Theme.deckTextBright
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                text: (root.trackLoaded ? root.effectiveKeyText : "--") + " ⌄"
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: root.trackLoaded && root.effectiveKey !== 0

                                onClicked: keyPickerMenu.popup()
                            }
                            // Lists the 12 keys of the SAME mode as the
                            // current effective key (transposing shifts
                            // pitch class, never major<->minor); picking
                            // one computes the shortest semitone delta
                            // and applies it via the pitch CO.
                            Menu {
                                id: keyPickerMenu

                                Repeater {
                                    model: 12

                                    MenuItem {
                                        readonly property int chromaticKey: (root.effectiveKey > 12 ? 13 : 1) + index

                                        checkable: true
                                        checked: chromaticKey === root.effectiveKey
                                        text: root.keyNames[chromaticKey]

                                        onTriggered: root.transposeToKey(chromaticKey)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        // UDD
        RowLayout {
            id: udd

            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            spacing: 8

            // UDDL: whole-track overview strip, matching deckA_mini_waveform
            // in Deo Pro dj_layout_spec.json -- moved here from its old
            // standalone row in DeckPanel.qml. Reuses Mixxx's own native
            // overview renderer via Deo.DeckOverviewWaveform.
            Deo.DeckOverviewWaveform {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 78
                group: root.group
            }
            // UDDR
            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 22
                spacing: 4

                InfoCell {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 50
                    label: "ELAPSED"
                    value: root.trackLoaded ? root.formatTime(durationControl.value * playPositionControl.value) : "--:--"
                }
                InfoCell {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 50
                    label: "REMAIN"
                    value: root.trackLoaded ? "-" + root.formatTime(durationControl.value * (1 - playPositionControl.value)) : "--:--"
                }
            }
        }
    }

    // Both Labels are Layout.fillWidth so they always take exactly this
    // cell's already-fixed (preferredWidth-weighted, not content-driven)
    // width; elide on the value label means a longer string (e.g. a long
    // key name) truncates in place instead of growing the cell -- the
    // whole point being that changing text content (a new BPM, a new key,
    // a ticking clock) never resizes the space allocated to it.
    component InfoCell: ColumnLayout {
        property string label: ""
        property string value: ""

        spacing: 0

        Label {
            Layout.fillWidth: true
            color: Theme.deckTextSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: 9
            text: parent.label
        }
        Label {
            Layout.fillWidth: true
            color: Theme.deckTextBright
            elide: Text.ElideRight
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 12
            text: parent.value
        }
    }

    // Subtly-boxed container for the small key controls under the KEY
    // readout (DeckTimeKeyMockup.qml's two lower mini-cells). Colors
    // derive from the deck's own accent so both cells always match the
    // deck's theme (the mockup's literal colors were arrangement
    // guidance only, per the user).
    component KeyMiniCell: Rectangle {
        color: Qt.darker(root.accentColor, 3.2)
        radius: 2
    }

    component KeyMiniButton: Rectangle {
        property alias text: buttonLabel.text
        signal clicked

        color: buttonMouseArea.pressed ? Qt.lighter(root.accentColor, 1.4) : root.accentColor
        height: 13
        radius: 2
        width: 16

        Label {
            id: buttonLabel

            anchors.centerIn: parent
            color: Theme.deckTextColor
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 10
        }
        MouseArea {
            id: buttonMouseArea

            anchors.fill: parent

            onClicked: parent.clicked()
        }
    }
}
