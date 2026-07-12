import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import ".." as Skin
import "../Theme"

// Milestone 2 pitch fader. Reuses stock, already-correct components where
// possible: Skin.RangeButton (cycles rateRange presets, answering the
// spec's "should range be switchable in-UI" question), and pitch_down/
// pitch_up nudge buttons + keylock toggle in the same arrangement as
// Deck/TempoColumn.qml.
//
// The fader itself is built directly on MixxxControls.Slider rather than
// Skin.ControlFader/Skin.Fader: those go through Fader.qml's `bar.margin`
// (a `property alias bar: barPath` into a ShapePath with inline custom
// properties), which fails to resolve at runtime once Mixxx.Controls is
// compiled into a QML plugin ("Cannot assign to non-existent property
// margin") even though the source is correct — a pre-existing issue in
// this pinned commit's QML module compilation, not something introduced
// here. Every stock skin control that reuses Skin.Fader hits the same
// wall, so this works around it with a from-scratch fill/handle visual
// instead of patching Qt's QML type compiler.
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
        opacity: root.trackLoaded ? 1.0 : 0.4

        Mixxx.ControlProxy {
            id: rateControl

            group: root.group
            key: "rate"
        }

        // Background track.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 4
            radius: 2
            color: Theme.knobBackgroundColor
        }
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
        // Fill between the center detent and the handle, so an off-center
        // fader is glanceable in the deck's accent color.
        Rectangle {
            readonly property real centerY: parent.height / 2
            readonly property real handleY: fader.topPadding + fader.visualPosition * fader.availableHeight

            x: parent.width / 2 - 2
            y: Math.min(centerY, handleY)
            width: 4
            height: Math.abs(handleY - centerY)
            color: root.accentColor
            z: 1
        }
        MixxxControls.Slider {
            id: fader

            anchors.fill: parent
            enabled: root.trackLoaded
            from: 0
            live: true
            orientation: Qt.Vertical
            value: rateControl.parameter

            handle: Rectangle {
                width: 28
                height: 10
                radius: 2
                color: Theme.white
                border.width: 1
                border.color: Theme.darkGray3

                x: (fader.width - width) / 2
                y: fader.topPadding + fader.visualPosition * fader.availableHeight - height / 2
            }

            onMoved: newValue => {
                rateControl.parameter = newValue;
            }

            TapHandler {
                onDoubleTapped: rateControl.reset()
            }
            TapHandler {
                acceptedButtons: Qt.RightButton

                onTapped: rateControl.reset()
            }
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
