import QtQuick 2.12
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// "Backspin" isn't a real Mixxx effect — EffectProcessor only ever sees an
// already-rendered buffer of forward-playing audio, with no access to the
// track's playback position or direction, so a genuine reverse+speed-ramp
// backspin cannot be implemented as an EffectProcessor plugin. This row is
// styled to match the real EffectSlot rows for visual consistency with the
// reference layout, but isn't a real EffectSlot: it triggers the engine's
// existing reverseroll control directly (hold = reverse + speed ramp,
// release = resume forward from where playback would naturally be), the
// same authentic reverse-playback mechanism real DJ software uses, with no
// new engine/DSP code.
Item {
    id: root

    required property string group
    required property color accentColor

    height: 50

    Mixxx.ControlProxy {
        id: reverseRollControl

        group: root.group
        key: "reverseroll"
    }

    Skin.Button {
        id: spinButton

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 5
        width: 40
        activeColor: root.accentColor
        highlight: reverseRollControl.value > 0
        text: "ON"

        onPressed: reverseRollControl.value = 1
        onReleased: reverseRollControl.value = 0
    }
    Label {
        anchors.left: spinButton.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 8
        color: Theme.deckTextColor
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
        text: "Backspin"
    }
    Label {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 8
        color: Theme.midGray3
        font.family: Theme.fontFamily
        font.pixelSize: 9
        text: "hold"
    }
}
