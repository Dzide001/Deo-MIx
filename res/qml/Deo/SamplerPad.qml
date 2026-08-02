import QtQuick 2.12
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// M4/pad-bank-switching: one Sampler-bank pad -- "basic playback" scope
// only (per explicit user decision: trigger whatever's already loaded into
// this Sampler slot via Mixxx's existing library/sampler UI elsewhere, no
// live recording/StemSwap/banks/groups, which would be a much larger,
// separate feature).
//
// Deliberately NOT res/qml/Sampler.qml (the existing full component with a
// gain knob, progress bar, VU meter, and drag-drop track loading) -- that
// component is designed as a wide horizontal row (implicitHeight ~50,
// wants several hundred px of width for its knob+button+label+progress+VU
// row), which doesn't fit this grid's small, roughly-square pad cells (as
// small as ~50px on the narrow deck side). This is a plain
// Skin.ControlButton instead, matching every other pad in this grid
// family (StemsBankContent's mute pads, HotcuesBankContent's hotcue
// pads) -- same toggleable-button idiom, just bound to this sampler
// slot's own `play` control.
Skin.ControlButton {
    id: root

    required property string group
    required property int samplerNumber

    // Mixxx.PlayerManager.getPlayer() allocates a brand-new QmlPlayerProxy
    // on every call with no persistent owner -- stored in its own property
    // (not read inline) so its trackChanged/isLoadedChanged notifications
    // keep firing instead of QML's GC reclaiming it right after one read.
    // Same pattern StemsBankContent.qml already uses for its own `player`.
    readonly property var player: Mixxx.PlayerManager.getPlayer(root.group)
    readonly property bool isLoaded: root.player ? root.player.isLoaded : false

    activeColor: Theme.samplerColor
    enabled: root.isLoaded
    group: root.group
    // ControlButton's own default `highlight: controlBehavior.isActive`
    // isn't reachable from outside (controlBehavior is private to that
    // file), so a separate proxy is needed to read the raw "play" value --
    // same reason StemsBankContent.qml's own pads each keep their own
    // mute-state proxy rather than relying on ControlButton's internal
    // default.
    highlight: enabled && playControl.value !== 0
    key: "play"
    opacity: enabled ? 1.0 : 0.35
    text: root.isLoaded ? root.player.currentTrack.title : ("S" + root.samplerNumber)
    toggleable: true

    Mixxx.ControlProxy {
        id: playControl

        group: root.group
        key: "play"
    }
}
