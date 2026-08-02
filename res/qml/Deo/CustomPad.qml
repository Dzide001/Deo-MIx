import QtQuick 2.12
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin

// M4/pad-bank-switching: one Custom-bank pad -- real, per-deck,
// user-assignable via a curated pick-from-a-list menu (right-click), NOT
// a live "click any control to learn it" system. Mixxx's MIDI-learning
// mechanism (ControllerLearningEventFilter) is Qt-Widgets-only (an event
// filter installed on legacy QWidgets, registered at skin-XML-parse time
// via LegacySkinParser) with no equivalent hook for QML components --
// building one would need a new global "last-touched control" C++
// singleton plus retrofitting a report-call into many existing QML
// widgets, a much bigger, separate undertaking than this feature itself.
//
// The actual assignment popup is NOT instantiated here -- CustomBankContent.qml
// owns ONE shared CustomActionPicker for all 8 pads (only one can ever be
// open at a time anyway) rather than each pad carrying its own Popup.
// Embedding a Popup directly inside a Repeater delegate turned out to
// break the delegate's own `index` context property entirely (Popups are
// reparented into Qt Quick Controls' overlay layer, outside the normal
// visual tree the Repeater manages) -- this pad just emits
// `assignRequested()` on right-click and lets the parent handle opening
// the shared picker positioned against this pad.
//
// Assignments persist via Mixxx.CustomPadSettings (a thin QML wrapper
// around CustomPadSettings/mixxx.cfg, src/preferences/custompadsettings.h),
// keyed per deck (root.deckGroup) and pad index, so each deck's 8 Custom
// pads have independent, restart-durable assignments.
Skin.ControlButton {
    id: root

    required property string deckGroup
    required property int padIndex
    required property color accentColor
    // Bumped by the parent (CustomBankContent.qml) after every assignment
    // change to force the read-only properties below to re-evaluate --
    // Q_INVOKABLE return values aren't otherwise trackable by QML's
    // binding system, unlike a real NOTIFY-bound Q_PROPERTY.
    property int revision: 0

    signal assignRequested

    readonly property string assignedGroup: {
        root.revision;
        return Mixxx.CustomPadSettings.getPadGroup(root.deckGroup, root.padIndex);
    }
    readonly property string assignedKey: {
        root.revision;
        return Mixxx.CustomPadSettings.getPadKey(root.deckGroup, root.padIndex);
    }
    readonly property string assignedLabel: {
        root.revision;
        return Mixxx.CustomPadSettings.getPadLabel(root.deckGroup, root.padIndex);
    }
    readonly property bool isAssigned: root.assignedKey !== ""
    // `toggleable` isn't persisted separately (only group/key/label are) --
    // re-derived by matching the stored (group, key) back against the
    // curated list that produced it. Falls back to false (momentary) if
    // no match is found, e.g. a future edit to CuratedActions.qml removes
    // an entry a pad was previously assigned from.
    readonly property bool assignedToggleable: {
        for (let i = 0; i < curatedActions.count; i++) {
            const entry = curatedActions.get(i);
            const entryGroup = entry.group === "" ? root.deckGroup : entry.group;
            if (entry.key === root.assignedKey && entryGroup === root.assignedGroup) {
                return entry.toggleable;
            }
        }
        return false;
    }

    // Deliberately NOT `enabled: root.isAssigned` -- Item.enabled cascades
    // to all descendants, including the right-click MouseArea below, which
    // would make an unassigned pad (the exact case a user needs to
    // right-click to assign in the first place) completely unclickable.
    // An empty group/key is already a safe no-op for the left-click
    // trigger path (QmlControlProxy's AllowMissingOrInvalid fallback), so
    // nothing needs to be functionally disabled here -- only the dimmed
    // look for "nothing assigned yet" is wanted, which opacity alone
    // already provides.
    activeColor: root.accentColor
    group: root.assignedGroup
    key: root.isAssigned ? root.assignedKey : ""
    opacity: root.isAssigned ? 1.0 : 0.35
    text: root.isAssigned ? root.assignedLabel : "+"
    toggleable: root.assignedToggleable

    Deo.CuratedActions {
        id: curatedActions
    }

    MouseArea {
        acceptedButtons: Qt.RightButton
        anchors.fill: parent

        onClicked: root.assignRequested()
    }
}
