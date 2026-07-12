import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// Deck shell: jog wheel + pitch fader (M2), VINYL/SLIP toggles, loop
// section (M2), and transport row. Two instances of this, mirrored
// left/right, make up the deck view.
Item {
    id: root

    required property string group
    required property string label
    required property color accentColor
    property bool mirrored: false

    readonly property bool trackLoaded: trackLoadedControl.value > 0

    implicitWidth: 320
    implicitHeight: content.implicitHeight

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

    ColumnLayout {
        id: content

        anchors.fill: parent

        LayoutMirroring.enabled: root.mirrored
        LayoutMirroring.childrenInherit: true
        spacing: 12

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
            spacing: 12

            Deo.JogWheel {
                id: jogWheel

                accentColor: root.accentColor
                group: root.group
                vinylMode: vinylToggle.checked
            }
            Deo.PitchFader {
                Layout.fillHeight: true
                accentColor: root.accentColor
                group: root.group
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Skin.Button {
                id: vinylToggle

                Layout.fillWidth: true
                activeColor: root.accentColor
                checkable: true
                checked: true
                text: "Vinyl"
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                activeColor: root.accentColor
                group: root.group
                key: "slip_enabled"
                text: "Slip"
                toggleable: true
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
