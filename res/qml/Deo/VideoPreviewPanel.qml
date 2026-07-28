import QtQuick 2.12
import QtQuick.Controls
import Mixxx 1.0 as Mixxx
import "../Theme"

// M12 Stage 3e: replaces Stage 3c/3d's inline preview Rectangle in
// MixerTabs.qml's VIDEO tab, which caused a real layout-shift bug --
// showing/hiding it resized the whole center mixer column, which pushed
// DECK A/DECK B around. Matches VirtualDJ's own pattern instead (per the
// user's reference screenshots): the live preview floats as an
// independent, draggable, minimizable panel over the waveform area, with
// no participation in any Layout at all -- that's what makes it immune to
// the resize bug, not just a different fixed position.
//
// Must be added as a top-level sibling of the root ColumnLayout in
// main.qml (not nested inside it), positioned with plain x/y rather than
// anchors/Layout attachments, so its drag doesn't fight a layout
// re-flowing it back.
Rectangle {
    id: root

    // Default position overlaps the top waveform strip, matching where
    // VirtualDJ's own floating preview sits by default.
    x: 40
    y: 40
    z: 100
    implicitWidth: minimized ? 160 : 320
    implicitHeight: minimized ? titleBar.height : titleBar.height + 240
    border.color: Theme.darkGray3
    border.width: 1
    color: Theme.deckBackgroundColor
    radius: 3
    visible: Mixxx.VideoEngine.enabled
    clip: true

    property bool minimized: false

    // Keep the panel reachable even after resizing the window smaller
    // than wherever it was last dragged to.
    onParentChanged: clampPosition()
    Connections {
        function onWidthChanged() {
            root.clampPosition();
        }
        function onHeightChanged() {
            root.clampPosition();
        }

        target: root.parent
    }

    function clampPosition() {
        if (!root.parent) {
            return;
        }
        x = Math.max(0, Math.min(x, root.parent.width - root.width));
        y = Math.max(0, Math.min(y, root.parent.height - root.height));
    }

    Behavior on implicitWidth {
        NumberAnimation {
            duration: 120
        }
    }
    Behavior on implicitHeight {
        NumberAnimation {
            duration: 120
        }
    }

    Rectangle {
        id: titleBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.darkGray2
        height: 22

        MouseArea {
            id: dragArea

            anchors.fill: parent
            cursorShape: Qt.SizeAllCursor
            drag.target: root
            drag.axis: Drag.XAndYAxis

            onReleased: root.clampPosition()
        }
        Label {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.deckTextSecondary
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 9
            text: "VIDEO PREVIEW"
        }
        // Minimize/restore control. A single glyph toggling between the
        // two states rather than two separate buttons -- title bar is
        // only 22px tall, no room for a labeled button.
        Rectangle {
            id: minimizeButton

            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            color: minimizeMouseArea.pressed ? Theme.accentColor : "transparent"
            height: 16
            radius: 2
            width: 16

            Label {
                anchors.centerIn: parent
                color: Theme.deckTextSecondary
                font.bold: true
                font.pixelSize: 11
                text: root.minimized ? "▢" : "–"
            }
            MouseArea {
                id: minimizeMouseArea

                anchors.fill: parent

                onClicked: root.minimized = !root.minimized
            }
        }
    }
    Mixxx.VideoPreview {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        visible: !root.minimized
    }
}
