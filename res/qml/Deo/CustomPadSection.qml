import QtQuick 2.12
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// DB's top slot: a user-assignable pad bank, matching the VirtualDJ
// reference's "Custom Buttons" concept (empty pads/knobs the user can wire
// to any action). Mixxx has no equivalent of VDJScript's action browser to
// build a real assignment UI against, and no per-pad persistence layer --
// scoped, per explicit user decision, as a visual placeholder only for
// the remaining 3 pads + slider, matching how StemPads.qml already
// handles its own not-yet-real slots (HiHat/Stems FX). Real assignment
// (pick a Mixxx ControlObject group+key per pad/slider, persist it, bind
// on click/drag) is deferred as its own future milestone -- see
// master_decision_list.md.
//
// The top-left pad is a real, wired Backspin trigger -- moved here from
// FXRack.qml's row 1, which used to be a dedicated non-swappable
// "Backspin effect slot" that read as broken next to the other two rows'
// real dropdowns. Backspin is a real, physics-based transport effect
// (see engine/controls/ratecontrol.cpp updateBackspin()) -- a continuous
// speed ramp from forward playback through zero to a fast reverse and
// back, not a simple reverse flip and not an EffectProcessor plugin
// (those never get access to playback position/speed, only an
// already-rendered forward-playing buffer) -- so it can't live in a real
// effect dropdown; a one-shot pad here is the closest real equivalent of
// this bank's "assignable action" concept there already was room for.
//
// Rebuilt per the user's reference mockup (CustomPadDesignMockup.qml) as
// a plain Item with the mockup's exact literal coordinates (it was drawn
// at DB upper half's real allocated size, 139x108, so these numbers are
// direct pixel targets, not a proportion to rederive) rather than
// QtQuick Layouts.
Item {
    id: root

    required property string group
    required property color accentColor

    // Fixed authored canvas, matching CustomPadDesignMockup.qml exactly --
    // DeckPanel.qml scales this to fit the real allocated box (a Scale
    // transform, not anchors.fill) rather than assuming the real box
    // already equals 139x108.
    height: 108
    width: 139

    // Geometry, taken directly from CustomPadDesignMockup.qml.
    readonly property real labelX: 8
    readonly property real labelWidth: 18
    readonly property real sliderX: 32
    readonly property real sliderY: 15
    readonly property real sliderWidth: 94
    readonly property real handleSize: 17
    readonly property real padRow1Y: 38
    readonly property real padRow2Y: 69
    readonly property real padHeight: 25
    readonly property real padLeftX: 32
    readonly property real padRightX: 84
    readonly property real padWidth: 42

    // Rotated label on the left edge, matching LoopSection's identical
    // treatment.
    Item {
        height: root.height
        width: root.labelWidth
        x: root.labelX
        y: 0

        Label {
            anchors.centerIn: parent
            color: Theme.deckLoopLabelColor
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 10
            rotation: -90
            text: "Custom"
        }
    }
    // Placeholder for a user-assignable slider/fader -- disabled rather
    // than invented, same reasoning as the pads below. A plain Rectangle
    // track + handle, not QtQuick.Controls' Slider (see PitchFader.qml's
    // header for the wall that hit trying to override that component's
    // own sizing).
    Rectangle {
        id: sliderTrack

        color: Theme.deckLineColor
        height: 5
        opacity: 0.35
        width: root.sliderWidth
        x: root.sliderX
        y: root.sliderY + root.handleSize / 2 - 2.5
    }
    Rectangle {
        color: Theme.deckTextSecondary
        height: root.handleSize
        opacity: 0.35
        radius: root.handleSize / 2
        width: root.handleSize / 3.4
        x: root.sliderX + root.sliderWidth * 0.05
        y: root.sliderY
    }
    // backspin_activate is a one-shot trigger, not a hold/release
    // control -- a plain onClicked/trigger() pair, same as the button
    // this replaced in FXRack.qml.
    Mixxx.ControlProxy {
        id: backspinActivateControl

        group: root.group
        key: "backspin_activate"
    }

    // 2x2 pad grid, 42x25 each, matching the reference mockup -- top-left
    // is the real Backspin trigger, the other three remain placeholders.
    Skin.Button {
        activeColor: root.accentColor
        height: root.padHeight
        text: ""
        width: root.padWidth
        x: root.padLeftX
        y: root.padRow1Y

        onClicked: backspinActivateControl.trigger()
    }
    Skin.Button {
        enabled: false
        height: root.padHeight
        opacity: 0.35
        text: ""
        width: root.padWidth
        x: root.padRightX
        y: root.padRow1Y
    }
    Skin.Button {
        enabled: false
        height: root.padHeight
        opacity: 0.35
        text: ""
        width: root.padWidth
        x: root.padLeftX
        y: root.padRow2Y
    }
    Skin.Button {
        enabled: false
        height: root.padHeight
        opacity: 0.35
        text: ""
        width: root.padWidth
        x: root.padRightX
        y: root.padRow2Y
    }
}
