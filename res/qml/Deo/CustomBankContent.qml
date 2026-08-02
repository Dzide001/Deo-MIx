import QtQuick 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo

// M4/pad-bank-switching: the Custom bank's content -- independently
// user-assignable pads (see CustomPad.qml), one per physical slot (8 wide,
// 4 narrow -- matching the narrow slot's original CustomPadSection.qml
// scope exactly). Persisted per deck (via Mixxx.CustomPadSettings), so
// this same grid shows a different set of assignments depending on which
// deck's dropdown it's reached through.
//
// Owns the ONE shared CustomActionPicker used by all pads (only one can
// ever be open at a time) -- see CustomPad.qml's own comment on why a
// Popup can't live inside the Repeater delegate itself.
//
// 8 hand-written instances rather than `Repeater{model:8}` -- see
// HotcuesBankContent.qml's own comment on why: a Repeater's `index`
// context property went undefined for this delegate specifically, for
// reasons that didn't resolve after several different fixes. Hand-written
// literal pads matches StemsBankContent.qml's own proven pattern.
Item {
    id: root

    required property string group
    required property color accentColor
    required property int columns
    required property int padCount

    // Bumped after every assignment change; each pad reads this back
    // (revision: root.revision) purely to force its own read-only
    // assignment properties to re-evaluate.
    property int revision: 0

    function openPickerFor(pad, padIndex) {
        picker.targetPadIndex = padIndex;
        picker.parent = pad;
        picker.x = (pad.width - picker.width) / 2;
        picker.y = pad.height;
        picker.open();
    }

    Deo.CuratedActions {
        id: curatedActionsForPicker
    }
    Deo.CustomActionPicker {
        id: picker

        property int targetPadIndex: -1

        actionsModel: curatedActionsForPicker

        onActionPicked: (group, key, label, toggleable) => {
            if (picker.targetPadIndex < 0) {
                return;
            }
            const resolvedGroup = group === "" ? root.group : group;
            Mixxx.CustomPadSettings.setPadAssignment(root.group, picker.targetPadIndex, resolvedGroup, key, label);
            root.revision++;
        }
        onClearRequested: {
            if (picker.targetPadIndex < 0) {
                return;
            }
            Mixxx.CustomPadSettings.clearPadAssignment(root.group, picker.targetPadIndex);
            root.revision++;
        }
    }

    GridLayout {
        anchors.fill: parent
        columns: root.columns
        columnSpacing: 6
        rowSpacing: 6

        Deo.CustomPad {
            id: pad0

            Layout.column: 0 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(0 / root.columns)
            accentColor: root.accentColor
            deckGroup: root.group
            padIndex: 0
            revision: root.revision
            visible: 0 < root.padCount

            onAssignRequested: root.openPickerFor(pad0, 0)
        }
        Deo.CustomPad {
            id: pad1

            Layout.column: 1 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(1 / root.columns)
            accentColor: root.accentColor
            deckGroup: root.group
            padIndex: 1
            revision: root.revision
            visible: 1 < root.padCount

            onAssignRequested: root.openPickerFor(pad1, 1)
        }
        Deo.CustomPad {
            id: pad2

            Layout.column: 2 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(2 / root.columns)
            accentColor: root.accentColor
            deckGroup: root.group
            padIndex: 2
            revision: root.revision
            visible: 2 < root.padCount

            onAssignRequested: root.openPickerFor(pad2, 2)
        }
        Deo.CustomPad {
            id: pad3

            Layout.column: 3 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(3 / root.columns)
            accentColor: root.accentColor
            deckGroup: root.group
            padIndex: 3
            revision: root.revision
            visible: 3 < root.padCount

            onAssignRequested: root.openPickerFor(pad3, 3)
        }
        Deo.CustomPad {
            id: pad4

            Layout.column: 4 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(4 / root.columns)
            accentColor: root.accentColor
            deckGroup: root.group
            padIndex: 4
            revision: root.revision
            visible: 4 < root.padCount

            onAssignRequested: root.openPickerFor(pad4, 4)
        }
        Deo.CustomPad {
            id: pad5

            Layout.column: 5 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(5 / root.columns)
            accentColor: root.accentColor
            deckGroup: root.group
            padIndex: 5
            revision: root.revision
            visible: 5 < root.padCount

            onAssignRequested: root.openPickerFor(pad5, 5)
        }
        Deo.CustomPad {
            id: pad6

            Layout.column: 6 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(6 / root.columns)
            accentColor: root.accentColor
            deckGroup: root.group
            padIndex: 6
            revision: root.revision
            visible: 6 < root.padCount

            onAssignRequested: root.openPickerFor(pad6, 6)
        }
        Deo.CustomPad {
            id: pad7

            Layout.column: 7 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(7 / root.columns)
            accentColor: root.accentColor
            deckGroup: root.group
            padIndex: 7
            revision: root.revision
            visible: 7 < root.padCount

            onAssignRequested: root.openPickerFor(pad7, 7)
        }
    }
}
