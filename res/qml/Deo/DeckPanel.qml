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
        RowLayout {
            id: body

            Layout.fillWidth: true
            Layout.fillHeight: true

            LayoutMirroring.enabled: root.mirrored
            LayoutMirroring.childrenInherit: true
            spacing: 10

            // Left column: FX rack + loop section.
            ColumnLayout {
                Layout.preferredWidth: 155
                Layout.fillHeight: true
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
                Item {
                    Layout.fillHeight: true
                }
            }
            // Right column: jog wheel/pitch fader + transport row.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 4

                    ColumnLayout {
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
                            implicitWidth: jogWheel.implicitWidth
                            implicitHeight: jogWheel.implicitHeight

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
                        Layout.preferredWidth: 34
                        accentColor: root.accentColor
                        group: root.group
                    }
                }
                Deo.TransportRow {
                    Layout.fillWidth: true
                    accentColor: root.accentColor
                    group: root.group
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
