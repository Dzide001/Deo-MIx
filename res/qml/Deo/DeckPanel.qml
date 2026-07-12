import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// Deck shell: FX rack (M3) beside jog wheel + pitch fader (M2), VINYL/SLIP
// toggles, loop section (M2), and transport row. Two instances of this,
// mirrored left/right, make up the deck view.
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

    // Drag an audio file from Finder onto the panel to load it; the M1
    // spec explicitly leaves the library/browser out of scope, so this is
    // the load path for exercising the acceptance criteria.
    Mixxx.PlayerDropArea {
        anchors.fill: parent
        group: root.group
        z: -1
    }

    RowLayout {
        id: layout

        anchors.fill: parent

        LayoutMirroring.enabled: root.mirrored
        LayoutMirroring.childrenInherit: true
        spacing: 8

        Deo.FXRack {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 130
            accentColor: root.accentColor
            group: root.group
            unitNumber: root.effectUnitNumber
        }
        ColumnLayout {
            spacing: 8

            RowLayout {
                Layout.fillWidth: true

                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: root.accentColor
                }
                Label {
                    color: Theme.deckTextColor
                    font.bold: true
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                    text: root.label
                }
                Item {
                    Layout.fillWidth: true
                }
                Label {
                    color: root.trackLoaded ? Theme.deckTextColor : Theme.midGray3
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.textFontPixelSize
                    text: root.trackLoaded ? "" : "No track loaded — drop a file here"
                }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4

                // Jog wheel with VINYL/SLIP as small pills overlaid in its
                // top-right corner, matching the reference layout, rather
                // than a full-width row underneath.
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
                    ColumnLayout {
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 8
                        spacing: 3
                        z: 2

                        Skin.Button {
                            id: vinylToggle

                            activeColor: root.accentColor
                            checkable: true
                            checked: true
                            implicitHeight: 18
                            implicitWidth: 50
                            text: "VINYL"
                        }
                        Skin.ControlButton {
                            activeColor: root.accentColor
                            group: root.group
                            implicitHeight: 18
                            implicitWidth: 50
                            key: "slip_enabled"
                            text: "SLIP"
                            toggleable: true
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
            Deo.LoopSection {
                Layout.fillWidth: true
                accentColor: root.accentColor
                group: root.group
            }
            Deo.TransportRow {
                Layout.fillWidth: true
                accentColor: root.accentColor
                group: root.group
            }
        }
    }
}
