import QtQuick 2.12
import QtQuick.Shapes 1.12
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// Deo-specific EQ column, rebuilt per the user's reference mockup
// (MixerDesignMockupA.qml) as a plain Item with the mockup's exact
// literal coordinates (its own 66x345 zone within the mockup's 297x378
// canvas, matching this column's real allocated size exactly). Same
// three EqKnobs + QuickFxKnob as before (HIHAT/MEL-VOX/KICK/FILTER,
// reusing the stock knobs via their `knob` alias), plus a compact
// round preset-cycle button replacing the previous inline ComboBox
// (no room for a text dropdown in this design -- tapping it cycles to
// the next quick-effect-chain preset instead of opening a list).
Item {
    id: root

    required property string group
    // true for the right-hand EQ column -- the preset button sits at a
    // different x in each column (leaning toward the mixer's own
    // center on both sides, per the mockup), not a shared/reusable x.
    required property bool rightSide

    width: 66
    height: 345

    readonly property real presetButtonX: root.rightSide ? 7 : 39

    Skin.EqKnob {
        x: 12
        y: 8
        width: 42
        height: 42
        knob.color: Theme.eqHighColor
        knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
        knob.height: 40
        knob.key: "parameter3"
        knob.width: 40
        statusKey: "button_parameter3"
    }
    Skin.EqKnob {
        x: 12
        y: 82
        width: 42
        height: 42
        knob.color: Theme.eqMidColor
        knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
        knob.height: 40
        knob.key: "parameter2"
        knob.width: 40
        statusKey: "button_parameter2"
    }
    Skin.EqKnob {
        x: 12
        y: 156
        width: 42
        height: 42
        knob.color: Theme.eqLowColor
        knob.group: "[EqualizerRack1_" + root.group + "_Effect1]"
        knob.height: 40
        knob.key: "parameter1"
        knob.width: 40
        statusKey: "button_parameter1"
    }
    Skin.QuickFxKnob {
        x: 12
        y: 230
        width: 42
        height: 42
        group: "[QuickEffectRack1_" + root.group + "]"
        knob.arcStyle: ShapePath.DashLine
        knob.arcStylePattern: [2, 2]
        knob.color: Theme.eqFxColor
        knob.height: 40
        knob.width: 40
    }

    Mixxx.ControlProxy {
        id: fxSelect

        group: "[QuickEffectRack1_" + root.group + "]"
        key: "loaded_chain_preset"
    }

    // Not Q_INVOKABLE on QmlChainPresetModel itself (rowCount() is a
    // plain QAbstractListModel override, not exposed to QML/JS) -- an
    // actual (invisible) ComboBox bound to the same model the stock
    // combo box used is the only way to get a usable item count.
    Skin.ComboBox {
        id: presetCountProbe

        model: Mixxx.EffectsManager.quickChainPresetModel
        visible: false
        width: 0
        height: 0
    }

    // Compact preset-cycle button, replacing the previous inline
    // ComboBox -- taps cycle to the next entry in the same real
    // quickChainPresetModel the ComboBox used to show.
    Skin.Button {
        x: root.presetButtonX
        y: 277
        width: 20
        height: 20

        onClicked: {
            const count = presetCountProbe.count;
            if (count <= 0) {
                return;
            }
            const current = fxSelect.value < 0 ? 0 : fxSelect.value;
            fxSelect.value = (current + 1) % count;
        }
    }
}
