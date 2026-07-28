import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import ".." as Skin
import "../Theme"

// DESIGN MOCKUP -- not used by the real app (nothing imports/instantiates
// this file). A visual-only stand-in for LoopSection.qml, with every real
// Mixxx.ControlProxy/required-group dependency stripped out and replaced
// with local, fake state, so it opens and previews correctly in Qt Design
// Studio / Qt Creator's Design mode -- the real LoopSection.qml can't,
// because it needs a real `group` supplied by the running app to render
// at all.
//
// Workflow: redesign this file freely in the visual tool (colors, sizes,
// spacing, layout -- whatever you want). When you're done, either tell
// Claude what changed in plain English, or just save the file -- the
// real LoopSection.qml will get the same visual/layout changes ported
// back in, with the real Mixxx wiring re-attached to whatever new
// structure you end up with.
RowLayout {
    id: root

    // width/height pinned to LoopSection's actual real-world rendered
    // size, not left to Design mode's default ~640x480 canvas -- the
    // real component only ever gets ~140x108px in the live app (deck
    // width -> DB's 22% share -> the 50/50 split with the Custom pad
    // section above it), computed at a representative ~1600x1100 window.
    // Without this, anything drawn to "fill" the canvas looks wildly
    // oversized once it's back in the real, much smaller allocated
    // space. This is an approximation -- the real size shifts with the
    // actual window size -- but it's close enough to design against.
    height: 108
    width: 139

    // Fake local state standing in for the real ControlProxy-backed
    // properties (trackLoaded, loopActive, etc.) in LoopSection.qml.
    property color accentColor: "#3C7993"
    property bool trackLoaded: true
    property bool loopActive: false
    property bool quantizeOn: true
    property var beatSizes: [1 / 32, 1 / 16, 1 / 8, 1 / 4, 1 / 2, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512]
    property int selectedIndex: 8

    spacing: 4

    function formatBeatSize(value) {
        if (value < 1) {
            return "1/" + Math.round(1 / value);
        }
        return Math.round(value).toString();
    }

    Menu {
        id: loopSettingsMenu

        MenuItem {
            checkable: true
            checked: root.quantizeOn
            text: "Quantize"

            onTriggered: root.quantizeOn = checked
        }
    }
    Item {
        Layout.fillHeight: true
        Layout.preferredWidth: root.width * 0.05
        Layout.minimumWidth: 14

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
            color: root.quantizeOn ? root.accentColor : Theme.deckLoopLabelColor
            height: 8
            radius: 4
            width: 8

            TapHandler {
                onTapped: loopSettingsMenu.popup(settingsDot)
            }
        }
    }
    GridLayout {
        Layout.fillHeight: true
        Layout.fillWidth: true
        columnSpacing: 2
        columns: 3
        rowSpacing: 4

        Skin.Button {
            activeColor: root.accentColor
            enabled: root.trackLoaded
            implicitHeight: 22
            implicitWidth: 21
            opacity: enabled ? 1.0 : 0.4
            text: "<"

            onClicked: root.selectedIndex = Math.max(0, root.selectedIndex - 1)
        }
        Skin.Button {
            activeColor: root.accentColor
            enabled: root.trackLoaded
            highlight: root.loopActive
            implicitHeight: 22
            implicitWidth: 21
            opacity: enabled ? 1.0 : 0.4
            text: root.formatBeatSize(root.beatSizes[root.selectedIndex])

            onClicked: root.loopActive = !root.loopActive
        }
        Skin.Button {
            activeColor: root.accentColor
            enabled: root.trackLoaded
            implicitHeight: 22
            implicitWidth: 21
            opacity: enabled ? 1.0 : 0.4
            text: ">"

            onClicked: root.selectedIndex = Math.min(root.beatSizes.length - 1, root.selectedIndex + 1)
        }
        Skin.Button {
            activeColor: root.accentColor
            enabled: root.trackLoaded
            implicitHeight: 24
            implicitWidth: 32
            opacity: enabled ? 1.0 : 0.4
            text: "In"
        }
        Skin.Button {
            activeColor: root.accentColor
            enabled: root.trackLoaded
            implicitHeight: 24
            implicitWidth: 32
            opacity: enabled ? 1.0 : 0.4
            text: "Out"
        }
        Skin.Button {
            activeColor: root.accentColor
            enabled: root.trackLoaded
            highlight: root.loopActive
            implicitHeight: 24
            implicitWidth: 32
            opacity: enabled ? 1.0 : 0.4
            text: root.loopActive ? "Exit" : "Rec"

            onClicked: root.loopActive = !root.loopActive
        }
    }
}
