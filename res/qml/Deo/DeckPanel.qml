import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// Deck shell, arranged per Deo Pro dj_layout_spec.json: a full-width
// track-info header, then a two-column body — [FX rack + loop section]
// beside [jog wheel/pitch fader + transport row] — rather than
// everything stacked in one column. Two instances of this, mirrored
// left/right, make up the deck view.
Item {
    id: root

    required property string group
    required property string label
    required property color accentColor
    required property int effectUnitNumber
    property bool mirrored: false

    readonly property bool trackLoaded: trackLoadedControl.value > 0

    implicitWidth: layout.implicitWidth
    implicitHeight: layout.implicitHeight

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -8
        color: Theme.deckPanelBackground
        z: -2
    }

    // Drag an audio file from Finder onto the panel to load it; the M1
    // spec explicitly leaves the library/browser out of scope, so this is
    // the load path for exercising the acceptance criteria.
    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: root.group
        z: -1
    }

    ColumnLayout {
        id: layout

        // Defensive containment: if internal content ever needs more room
        // than this panel was allocated (e.g. a minimum-width mismatch
        // between here and a child several levels down), clip instead of
        // visually bleeding into whatever's rendered next to it. Applied
        // here rather than on root so it doesn't also clip the background
        // Rectangle's intentional -8 margin bleed above.
        clip: true
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            LayoutMirroring.enabled: root.mirrored
            LayoutMirroring.childrenInherit: true

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: root.accentColor
            }
            Label {
                color: Theme.deckTextBright
                font.bold: true
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textFontPixelSize
                text: root.label
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                color: root.trackLoaded ? Theme.deckTextBright : Theme.deckTextSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textFontPixelSize
                text: root.trackLoaded ? "" : "No track loaded — drop a file here"
            }
        }
        // Whole-track overview strip, matching deckA_mini_waveform in
        // Deo Pro dj_layout_spec.json — a small "static" preview that
        // lives inside the deck itself, between the track-info header and
        // the FX/jog body, distinct from the shared full-width scrolling
        // waveform in main.qml.
        Deo.DeckOverviewWaveform {
            Layout.fillWidth: true
            Layout.preferredHeight: 18
            group: root.group
        }
        RowLayout {
            id: body

            Layout.fillWidth: true
            Layout.fillHeight: true

            LayoutMirroring.enabled: root.mirrored
            LayoutMirroring.childrenInherit: true
            spacing: 10

            // Left column: FX rack + loop section. 46/54 split matches
            // deckA_fx_cluster (46%) vs deckA_jog_row (54%) in
            // Deo Pro dj_layout_spec.json — both the top_controls and
            // bottom_row halves use the same split, which is why one
            // full-height two-column body (FX+Loop stacked left,
            // jog+pitch+Transport stacked right) is equivalent to the
            // spec's two separate same-split rows. Deliberately a plain
            // preferredWidth *weight* (matching 46/54 at a representative
            // ~624px deck width), not `body.width * 0.46` -- see the note
            // on deckMixerRow in main.qml for why that pattern is both
            // circular and doesn't actually shrink to fit.
            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.minimumWidth: 150
                Layout.preferredWidth: 287
                spacing: 8

                Deo.FXRack {
                    Layout.fillWidth: true
                    accentColor: root.accentColor
                    group: root.group
                    unitNumber: root.effectUnitNumber
                }
                Deo.LoopSection {
                    Layout.fillWidth: true
                    accentColor: root.accentColor
                    group: root.group
                }
                // M4: stem pads, matching deckA_pads_block's position
                // alongside the loop row in the spec (a sibling of
                // deckA_loop_row within deckA_loop_pads_combo) as closely
                // as this deck's simpler stacked-column structure allows.
                Deo.StemPads {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.minimumHeight: 60
                    accentColor: root.accentColor
                    group: root.group
                }
            }
            // Right column: jog wheel/pitch fader + transport row.
            ColumnLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Layout.preferredWidth: 337
                spacing: 8

                RowLayout {
                    id: jogPitchRow

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    spacing: 4

                    // deckA_jogwheel_area / deckA_pitch_fader_area are
                    // 68%/32% of this row's width -- same fixed-weight
                    // pattern as above, not `jogPitchRow.width * 0.68`.
                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.preferredWidth: 229
                        spacing: 4

                        // VINYL/SLIP row above the jog wheel, matching the
                        // reference, plus an eject icon overlaid on the
                        // wheel's top-left corner.
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 3

                            Skin.Button {
                                id: vinylToggle

                                activeColor: root.accentColor
                                checkable: true
                                checked: true
                                implicitHeight: 16
                                implicitWidth: 22
                                text: "V"
                            }
                            Skin.ControlButton {
                                activeColor: root.accentColor
                                group: root.group
                                implicitHeight: 16
                                implicitWidth: 22
                                key: "slip_enabled"
                                text: "S"
                                toggleable: true
                            }
                        }
                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Deo.JogWheel {
                                id: jogWheel

                                anchors.fill: parent
                                accentColor: root.accentColor
                                group: root.group
                                vinylMode: vinylToggle.checked
                            }
                            Skin.ControlButton {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.margins: 6
                                width: 20
                                height: 20
                                activeColor: root.accentColor
                                group: root.group
                                key: "eject"
                                text: "⏏"
                                z: 2
                            }
                        }
                    }
                    Deo.PitchFader {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        Layout.preferredWidth: 108
                        accentColor: root.accentColor
                        group: root.group
                    }
                }
                Deo.TransportRow {
                    Layout.fillWidth: true
                    accentColor: root.accentColor
                    group: root.group
                }
            }
        }
    }
}
