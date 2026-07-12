import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import Mixxx.Controls 1.0 as MixxxControls
import "." as Deo
import ".." as Skin
import "../Theme"

// Milestone 3 FX rack, restructured per Deo Pro dj_layout_spec.json to
// match the reference's column layout: a rotated "FX" label, then three
// parallel columns (dropdowns / sliders / gear icons), each with one row
// per slot — not one row per slot bundling all three the way stock
// Skin.EffectSlot lays things out (see git history for that version).
// Built directly on Mixxx.EffectSlotProxy rather than Skin.EffectSlot so
// each piece (selector/meta/expand) can live in its own column. Each
// dropdown row has a small enable toggle to its left, matching the
// reference's effects-list popup (toggle switch beside each name).
//
// Row 1 is Backspin; rows 2-3 are two real slots. unitNumber permanently
// routes to this deck's channel via group_[ChannelN]_enable.
Item {
    id: root

    required property string group
    required property int unitNumber
    required property color accentColor

    readonly property bool trackLoaded: trackLoadedControl.value > 0
    readonly property Mixxx.EffectSlotProxy slot1: Mixxx.EffectsManager.getEffectSlot(unitNumber, 1)
    readonly property Mixxx.EffectSlotProxy slot2: Mixxx.EffectsManager.getEffectSlot(unitNumber, 2)
    property int expandedSlot: 0 // 0 = none, 1 or 2 = that slot's parameters shown

    implicitWidth: 155
    implicitHeight: mainColumn.implicitHeight
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
    ListModel {
        id: backspinOnlyModel

        ListElement {
            display: "Backspin"
            effectId: "backspin"
        }
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

    ColumnLayout {
        id: mainColumn

        anchors.fill: parent
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 2

            // Rotated "FX" label on the left edge.
            Item {
                Layout.preferredWidth: 14
                Layout.fillHeight: true

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
            // Dropdowns column: each row is a small enable toggle beside
            // the effect selector, matching the reference's effects list.
            ColumnLayout {
                Layout.preferredWidth: 76
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    // Backspin is a real, physics-based transport effect
                    // (see engine/controls/ratecontrol.cpp updateBackspin())
                    // — a continuous speed ramp from forward playback
                    // through zero to a fast reverse and back, not a simple
                    // reverse flip and not an EffectProcessor plugin
                    // (those never get access to playback position/speed,
                    // only an already-rendered forward-playing buffer).
                    // backspin_activate is a one-shot trigger, not a
                    // hold/release control.
                    Skin.ControlButton {
                        Layout.preferredWidth: 16
                        Layout.fillHeight: true
                        activeColor: root.accentColor
                        enabled: false
                        group: root.group
                        key: "backspin_enabled"
                        text: ""
                        toggleable: true

                        Mixxx.ControlProxy {
                            id: backspinActivateControl

                            group: root.group
                            key: "backspin_activate"
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24

                        Skin.ComboBox {
                            id: backspinCombo

                            anchors.fill: parent
                            currentIndex: 0
                            model: backspinOnlyModel
                            textRole: "display"
                        }
                        // A real dropdown with a single fixed entry has
                        // nothing useful to open; this overlay makes the
                        // whole row a one-shot trigger instead, visually
                        // consistent with the other two dropdown rows.
                        MouseArea {
                            anchors.fill: parent

                            onClicked: backspinActivateControl.trigger()
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Skin.ControlButton {
                        Layout.preferredWidth: 16
                        Layout.fillHeight: true
                        activeColor: root.accentColor
                        group: root.slot1.group
                        key: "enabled"
                        text: ""
                        toggleable: true
                    }
                    Skin.ComboBox {
                        id: slot1Selector

                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        model: curatedEffects
                        textRole: "display"

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
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Skin.ControlButton {
                        Layout.preferredWidth: 16
                        Layout.fillHeight: true
                        activeColor: root.accentColor
                        group: root.slot2.group
                        key: "enabled"
                        text: ""
                        toggleable: true
                    }
                    Skin.ComboBox {
                        id: slot2Selector

                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        model: curatedEffects
                        textRole: "display"

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
                }
            }
            // Sliders column: metaknob for each real slot, exposed as a
            // horizontal slider to match the reference (stock
            // Skin.EffectSlot uses a knob for this instead).
            ColumnLayout {
                Layout.preferredWidth: 55
                spacing: 2

                Item {
                    // Backspin has no continuous parameter.
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                }
                Deo.EffectMetaSlider {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    accentColor: root.accentColor
                    group: root.slot1.group
                }
                Deo.EffectMetaSlider {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    accentColor: root.accentColor
                    group: root.slot2.group
                }
            }
            // Gear column: toggles the shared expanded-parameters row
            // below for that slot.
            ColumnLayout {
                Layout.preferredWidth: 18
                spacing: 2

                Item {
                    // Backspin has no parameters to expand.
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                }
                Skin.Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    activeColor: root.accentColor
                    highlight: root.expandedSlot === 1
                    text: "⚙"

                    onClicked: root.expandedSlot = (root.expandedSlot === 1 ? 0 : 1)
                }
                Skin.Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    activeColor: root.accentColor
                    highlight: root.expandedSlot === 2
                    text: "⚙"

                    onClicked: root.expandedSlot = (root.expandedSlot === 2 ? 0 : 2)
                }
            }
        }
        Deo.EffectParametersRow {
            Layout.fillWidth: true
            accentColor: root.accentColor
            slot: root.expandedSlot === 1 ? root.slot1 : root.expandedSlot === 2 ? root.slot2 : null
            visible: root.expandedSlot !== 0
        }
    }
}
