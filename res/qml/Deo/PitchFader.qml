import QtQuick 2.12
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import ".." as Skin
import "../Theme"

// Milestone 2 pitch fader, rebuilt per the user's reference mockup
// (FaderSectionDesignMockup.qml) as a plain Item with the mockup's exact
// literal coordinates (it was drawn at DD's real allocated size, 44x234,
// so these numbers are direct pixel targets, not a proportion to
// rederive), rather than QtQuick Layouts.
//
// The fader itself is built directly on MixxxControls.Slider rather than
// Skin.ControlFader/Skin.Fader: those go through Fader.qml's `bar.margin`
// (a `property alias bar: barPath` into a ShapePath with inline custom
// properties), which fails to resolve at runtime once Mixxx.Controls is
// compiled into a QML plugin ("Cannot assign to non-existent property
// margin") even though the source is correct -- a pre-existing issue in
// this pinned commit's QML module compilation, not something introduced
// here. Every stock skin control that reuses Skin.Fader hits the same
// wall, so this works around it with a from-scratch fill/handle visual
// instead of patching Qt's QML type compiler.
Item {
    id: root

    required property string group
    required property color accentColor

    // Fixed authored canvas, matching FaderSectionDesignMockup.qml exactly
    // -- DeckPanel.qml scales this to fit the real allocated box (a Scale
    // transform, not anchors.fill) rather than assuming the real box
    // already equals 44x234.
    height: 234
    width: 44

    readonly property bool trackLoaded: trackLoadedControl.value > 0
    readonly property real percentOffCenter: (rateRatioControl.value - 1) * 100

    // Geometry, taken directly from FaderSectionDesignMockup.qml.
    readonly property real frameX: 4
    readonly property real frameY: 6
    readonly property real frameWidth: 37
    readonly property real frameHeight: 180
    readonly property real readoutX: 6
    readonly property real readoutY: 13
    readonly property real readoutWidth: 32
    readonly property real readoutHeight: 22
    readonly property real trackX: 21
    readonly property real trackY: 49
    readonly property real trackWidth: 3
    readonly property real trackHeight: 127
    readonly property real buttonsY: 192
    readonly property real buttonsHeight: 15
    readonly property real minusX: 4
    readonly property real plusX: 24
    readonly property real buttonWidth: 17

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
    Mixxx.ControlProxy {
        id: rateControl

        group: root.group
        key: "rate"
    }

    // Background frame/card, matching the mockup's recessed-panel look
    // behind both the readout and the fader track.
    Rectangle {
        color: "#0f2e3f"
        height: root.frameHeight
        opacity: root.trackLoaded ? 1.0 : 0.4
        radius: 4
        width: root.frameWidth
        x: root.frameX
        y: root.frameY
    }
    // Readout box, restyled to match the mockup's inset dark pill instead
    // of the previous plain text-only label.
    Rectangle {
        color: "#071e2b"
        height: root.readoutHeight
        radius: 9
        width: root.readoutWidth
        x: root.readoutX
        y: root.readoutY

        Label {
            anchors.centerIn: parent
            color: Math.abs(root.percentOffCenter) > 0.05 ? root.accentColor : Theme.deckTextColor
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: Theme.textFontPixelSize
            text: (root.percentOffCenter >= 0 ? "+" : "") + root.percentOffCenter.toFixed(1)
        }
    }
    // Fader track + handle, drawn from scratch (see file header for why),
    // sized to the frame's real hit area rather than just the thin
    // visual centerline.
    Item {
        height: root.trackHeight
        opacity: root.trackLoaded ? 1.0 : 0.4
        width: root.frameWidth
        x: root.frameX
        y: root.trackY

        // Thin visual centerline.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.knobBackgroundColor
            height: parent.height
            radius: 1.5
            width: 3
        }
        // Center-detent tick so the neutral point is visible regardless
        // of where the fader handle currently is.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.midGray3
            height: 2
            width: 11
            z: 1
        }
        // Fill between the center detent and the handle, so an
        // off-center fader is glanceable in the deck's accent color.
        Rectangle {
            readonly property real centerY: parent.height / 2
            readonly property real handleY: fader.topPadding + fader.visualPosition * fader.availableHeight

            color: root.accentColor
            height: Math.abs(handleY - centerY)
            width: 3
            x: parent.width / 2 - 1.5
            y: Math.min(centerY, handleY)
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
                border.color: Theme.darkGray3
                border.width: 1
                color: "#354d5b"
                height: 10
                radius: 5
                width: 17

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
    // deckA_pitch_fader_area's spec composition is exactly these three
    // pieces (readout / fader / minus_plus) -- a keylock ("KEY") and
    // range ("RNG") button pair used to live here too, which pushed this
    // column's content past its allocated height and cut it off at the
    // bottom (not in the spec, so removed rather than resized).
    Skin.ControlButton {
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        height: root.buttonsHeight
        key: "pitch_down"
        opacity: enabled ? 1.0 : 0.4
        text: "-"
        width: root.buttonWidth
        x: root.minusX
        y: root.buttonsY
    }
    Skin.ControlButton {
        activeColor: root.accentColor
        enabled: root.trackLoaded
        group: root.group
        height: root.buttonsHeight
        key: "pitch_up"
        opacity: enabled ? 1.0 : 0.4
        text: "+"
        width: root.buttonWidth
        x: root.plusX
        y: root.buttonsY
    }
}
