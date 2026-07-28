import QtQuick 2.12
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// Milestone 3 FX rack, rebuilt per the user's reference mockup
// (EffectsSectionMockup.qml) as a plain Item with the mockup's exact
// literal coordinates (it was drawn at DA-upper's real allocated size,
// 252x108, so these numbers are direct pixel targets, not a proportion to
// rederive) rather than QtQuick Layouts. This lets every element sit at
// the mockup's exact drawn position without fighting GridLayout's
// shared-column-width behavior.
//
// All three rows are real effect slots (1/2/3 of this unit's 4 available
// slots, per effects/defs.h's kNumEffectsPerUnit) -- row 1 used to be a
// dedicated, non-swappable Backspin trigger, but per explicit user
// decision that felt broken next to rows 2/3's real dropdowns (clicking
// it never opened a popup like the others do); Backspin itself (a real
// physics-based transport effect, not a pluggable EffectProcessor -- see
// CustomPadSection.qml) moved to a real pad there instead. unitNumber
// permanently routes to this deck's channel via group_[ChannelN]_enable.
// Per explicit user decision, the real enable-toggle for all three slots
// (a working effect on/off switch) is kept even though the mockup didn't
// draw it -- fit into the small gap between the rotated label and the
// effect dropdown.
Item {
    id: root

    required property string group
    required property int unitNumber
    required property color accentColor

    // Fixed authored canvas, matching EffectsSectionMockup.qml exactly --
    // DeckPanel.qml scales this to fit the real allocated box (a Scale
    // transform, not anchors.fill) rather than assuming the real box
    // already equals 252x108.
    height: 108
    width: 252

    readonly property bool trackLoaded: trackLoadedControl.value > 0
    readonly property Mixxx.EffectSlotProxy slot1: Mixxx.EffectsManager.getEffectSlot(unitNumber, 1)
    readonly property Mixxx.EffectSlotProxy slot2: Mixxx.EffectsManager.getEffectSlot(unitNumber, 2)
    readonly property Mixxx.EffectSlotProxy slot3: Mixxx.EffectsManager.getEffectSlot(unitNumber, 3)
    property int expandedSlot: 0 // 0 = none, 1/2/3 = that slot's parameters shown

    // Row/column geometry, taken directly from the latest
    // EffectsSectionMockup.qml (combo narrowed/shifted to make room for a
    // wider enable-toggle, sliders now real controls at their own
    // y/height rather than filling the whole row).
    readonly property real row1Y: 5
    readonly property real row2Y: 38
    readonly property real row3Y: 71
    readonly property real rowHeight: 27
    readonly property real labelX: 3
    readonly property real labelWidth: 18
    readonly property real toggleX: 31
    readonly property real toggleWidth: 17
    readonly property real comboX: 58
    readonly property real comboWidth: 82
    readonly property real sliderX: 154
    readonly property real sliderWidth: 61
    readonly property real sliderHeight: 19
    readonly property real slider1Y: 9
    readonly property real slider2Y: 43
    readonly property real slider3Y: 75
    readonly property real gearX: 224
    readonly property real gearWidth: 24

    enabled: root.trackLoaded
    opacity: enabled ? 1.0 : 0.4

    Mixxx.ControlProxy {
        id: trackLoadedControl

        group: root.group
        key: "track_loaded"
    }
    // Pin this unit to this deck's channel permanently; M3 is a fixed
    // per-deck rack, not a flexible any-unit-to-any-channel router.
    Mixxx.ControlProxy {
        id: routingControl

        group: "[EffectRack1_EffectUnit" + root.unitNumber + "]"
        key: "group_" + root.group + "_enable"

        onInitializedChanged: {
            value = 1;
        }
    }
    Deo.CuratedEffectsModel {
        id: curatedEffects
    }

    function syncComboBox(comboBox, slot) {
        const rowCount = comboBox.model.rowCount();
        for (let i = 0; i < rowCount; i++) {
            if (comboBox.model.get(i).effectId === slot.effectId) {
                comboBox.currentIndex = i;
                return;
            }
        }
        comboBox.currentIndex = -1;
    }

    // Rotated "FX" label on the left edge.
    Item {
        height: root.height
        width: root.labelWidth
        x: root.labelX
        y: 0

        Label {
            anchors.centerIn: parent
            color: Theme.deckTextSecondary
            font.bold: true
            font.family: Theme.fontFamily
            font.pixelSize: 10
            rotation: -90
            text: "FX"
        }
    }

    // Row 1: real effect slot 3 -- was a dedicated, non-swappable
    // Backspin trigger; per explicit user decision that read as broken
    // next to rows 2/3's real dropdowns, so this is now a real slot
    // exactly like them (Backspin moved to a real pad in
    // CustomPadSection.qml instead).
    Skin.ControlButton {
        activeColor: root.accentColor
        group: root.slot3.group
        height: root.rowHeight
        key: "enabled"
        text: ""
        toggleable: true
        width: root.toggleWidth
        x: root.toggleX
        y: root.row1Y
    }
    Skin.ComboBox {
        id: slot3Selector

        // Same height as everything else in the row -- no +8/-4
        // compensation (see rows 2/3's comment for why that trick was
        // removed).
        height: root.rowHeight
        model: curatedEffects
        textRole: "display"
        width: root.comboWidth
        x: root.comboX
        y: root.row1Y

        Component.onCompleted: root.syncComboBox(slot3Selector, root.slot3)
        onActivated: index => {
            root.slot3.effectId = model.get(index).effectId;
        }

        Connections {
            function onEffectIdChanged() {
                root.syncComboBox(slot3Selector, root.slot3);
            }

            target: root.slot3
        }
    }
    Deo.EffectMetaSlider {
        accentColor: root.accentColor
        group: root.slot3.group
        height: root.sliderHeight
        width: root.sliderWidth
        x: root.sliderX
        y: root.slider1Y
    }
    Skin.Button {
        activeColor: root.accentColor
        height: root.rowHeight
        highlight: root.expandedSlot === 3
        text: ""
        width: root.gearWidth
        x: root.gearX
        y: root.row1Y

        onClicked: root.expandedSlot = (root.expandedSlot === 3 ? 0 : 3)

        Image {
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            height: 11
            source: "../../../images/gear.png"
            width: 11
        }
    }

    // Row 2: real effect slot 1.
    Skin.ControlButton {
        activeColor: root.accentColor
        group: root.slot1.group
        height: root.rowHeight
        key: "enabled"
        text: ""
        toggleable: true
        width: root.toggleWidth
        x: root.toggleX
        y: root.row2Y
    }
    Skin.ComboBox {
        id: slot1Selector

        // Same height as everything else in the row -- no +8/-4
        // compensation (see the backspin row's comment above for why
        // that trick was removed).
        height: root.rowHeight
        model: curatedEffects
        textRole: "display"
        width: root.comboWidth
        x: root.comboX
        y: root.row2Y

        Component.onCompleted: root.syncComboBox(slot1Selector, root.slot1)
        onActivated: index => {
            root.slot1.effectId = model.get(index).effectId;
        }

        Connections {
            function onEffectIdChanged() {
                root.syncComboBox(slot1Selector, root.slot1);
            }

            target: root.slot1
        }
    }
    Deo.EffectMetaSlider {
        accentColor: root.accentColor
        group: root.slot1.group
        height: root.sliderHeight
        width: root.sliderWidth
        x: root.sliderX
        y: root.slider2Y
    }
    Skin.Button {
        activeColor: root.accentColor
        height: root.rowHeight
        highlight: root.expandedSlot === 1
        text: ""
        width: root.gearWidth
        x: root.gearX
        y: root.row2Y

        onClicked: root.expandedSlot = (root.expandedSlot === 1 ? 0 : 1)

        Image {
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            height: 11
            source: "../../../images/gear.png"
            width: 11
        }
    }

    // Row 3: real effect slot 2.
    Skin.ControlButton {
        activeColor: root.accentColor
        group: root.slot2.group
        height: root.rowHeight
        key: "enabled"
        text: ""
        toggleable: true
        width: root.toggleWidth
        x: root.toggleX
        y: root.row3Y
    }
    Skin.ComboBox {
        id: slot2Selector

        // Same height as everything else in the row -- no +8/-4
        // compensation (see the backspin row's comment above for why
        // that trick was removed).
        height: root.rowHeight
        model: curatedEffects
        textRole: "display"
        width: root.comboWidth
        x: root.comboX
        y: root.row3Y

        Component.onCompleted: root.syncComboBox(slot2Selector, root.slot2)
        onActivated: index => {
            root.slot2.effectId = model.get(index).effectId;
        }

        Connections {
            function onEffectIdChanged() {
                root.syncComboBox(slot2Selector, root.slot2);
            }

            target: root.slot2
        }
    }
    Deo.EffectMetaSlider {
        accentColor: root.accentColor
        group: root.slot2.group
        height: root.sliderHeight
        width: root.sliderWidth
        x: root.sliderX
        y: root.slider3Y
    }
    Skin.Button {
        activeColor: root.accentColor
        height: root.rowHeight
        highlight: root.expandedSlot === 2
        text: ""
        width: root.gearWidth
        x: root.gearX
        y: root.row3Y

        onClicked: root.expandedSlot = (root.expandedSlot === 2 ? 0 : 2)

        Image {
            anchors.centerIn: parent
            fillMode: Image.PreserveAspectFit
            height: 11
            source: "../../../images/gear.png"
            width: 11
        }
    }

    Deo.EffectParametersRow {
        accentColor: root.accentColor
        height: 50
        slot: root.expandedSlot === 1 ? root.slot1 : root.expandedSlot === 2 ? root.slot2 : root.expandedSlot === 3 ? root.slot3 : null
        visible: root.expandedSlot !== 0
        width: root.width
        x: 0
        y: root.row3Y + root.rowHeight
    }
}
