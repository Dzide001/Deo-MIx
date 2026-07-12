import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// Milestone 2 pitch fader. Reuses stock, already-correct components:
// Skin.ControlFader (key "rate", double-click/right-click reset to center
// built in), Skin.RangeButton (cycles rateRange presets, answering the
// spec's "should range be switchable in-UI" question), and pitch_down/
// pitch_up nudge buttons + keylock toggle in the same arrangement as
// Deck/TempoColumn.qml.
ColumnLayout {
    id: root

    required property string group
    required property color accentColor

    readonly property bool trackLoaded: trackLoadedControl.value > 0
    readonly property real percentOffCenter: (rateRatioControl.value - 1) * 100

    spacing: 6

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: rateRatioControl

        group: root.group
        key: "rate_ratio"
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        color: Math.abs(root.percentOffCenter) > 0.05 ? root.accentColor : Theme.deckTextColor
        font.bold: true
        font.family: Theme.fontFamily
        font.pixelSize: Theme.textFontPixelSize
        text: (root.percentOffCenter >= 0 ? "+" : "") + root.percentOffCenter.toFixed(1)
    }
    Item {
        Layout.alignment: Qt.AlignHCenter
        Layout.fillHeight: true
        Layout.minimumHeight: 140
        implicitWidth: 40

        // Center-detent tick so the neutral point is visible regardless of
        // where the fader handle currently is.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 2
            color: Theme.midGray3
            z: 1
        }
        Skin.ControlFader {
            id: fader

            anchors.fill: parent
            bar.start: 0.5
            enabled: root.trackLoaded
            group: root.group
            key: "rate"
            opacity: enabled ? 1.0 : 0.4
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 4

        Skin.ControlButton {
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.trackLoaded
            group: root.group
            key: "pitch_down"
            opacity: enabled ? 1.0 : 0.4
            text: "-"
        }
        Skin.ControlButton {
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.trackLoaded
            group: root.group
            key: "keylock"
            opacity: enabled ? 1.0 : 0.4
            text: "Lock"
            toggleable: true
        }
        Skin.ControlButton {
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.trackLoaded
            group: root.group
            key: "pitch_up"
            opacity: enabled ? 1.0 : 0.4
            text: "+"
        }
    }
    Skin.RangeButton {
        Layout.fillWidth: true
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        opacity: enabled ? 1.0 : 0.4
    }
}
