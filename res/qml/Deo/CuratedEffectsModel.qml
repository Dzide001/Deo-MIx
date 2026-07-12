import QtQuick 2.12
import Mixxx 1.0 as Mixxx

// Filters Mixxx.EffectsManager.visibleEffectsModel down to the real
// (non-transport) effects from the reference's curated "Default" list.
// "Cut" and "Echo Out" don't have Mixxx equivalents, so they're left out
// rather than guessed at. Backspin and the other pseudo-effects
// (BrakeStart, Beat Grid, Loop Out, Scale Down, Slip Roll) aren't real
// EffectSlot entries at all — see BackspinSlot.qml — so they don't belong
// in this list regardless.
//
// This intentionally filters at the skin layer rather than writing to
// Mixxx's own visible-effects preference (effects.xml): that preference
// is still fully user-editable via Preferences -> Effects (already
// reachable via the gear icon in main.qml) and this leaves it untouched.
ListModel {
    id: root

    readonly property var allowedNames: ["Echo", "Distortion", "Flanger", "Phaser", "Reverb"]

    function rebuild() {
        root.clear();
        const source = Mixxx.EffectsManager.visibleEffectsModel;
        const count = source.rowCount();
        for (let i = 0; i < count; i++) {
            const entry = source.get(i);
            if (root.allowedNames.includes(entry.display)) {
                root.append({
                    "effectId": entry.effectId,
                    "display": entry.display
                });
            }
        }
    }

    Component.onCompleted: rebuild()
}
