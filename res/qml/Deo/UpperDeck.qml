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
            // UDTR
            RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 22
                spacing: 4

                InfoCell {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 50
                    label: "BPM"
                    value: root.trackLoaded && bpmControl.value > 0 ? bpmControl.value.toFixed(1) : "--"
                }
                InfoCell {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 50
                    label: "KEY"
                    value: root.trackLoaded && root.currentTrack?.keyText ? root.currentTrack.keyText : "--"
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
}
