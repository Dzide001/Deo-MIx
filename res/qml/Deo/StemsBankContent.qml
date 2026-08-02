import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin

// M4/pad-bank-switching: the Stems bank's content, extracted from the old
// StemPads.qml (which used to own the whole pad section, including the
// "PADS" label and bank-switcher dropdown -- both now live once in the
// shared PadBankSection.qml shell instead, so this file only owns the 8
// pads themselves). See StemPads.qml's git history for the original
// single-file version and its full rationale.
//
// Bound to Mixxx's real native STEM playback (2.6+, pre-separated 4-track
// STEM files: Vocals/Drums/Bass/Melody-Other). The reference screenshot's
// Kick/HiHat pads aren't real -- no stem model isolates individual drum
// hits from within a mix, only a combined Drums stem exists -- so this
// grid is relabeled to the 4 real stems (Vocal/Instru/Bass/Drums) plus two
// one-press combo pads built from real per-stem mutes (Acapella = mute all
// but vocal; Instrumental = mute vocal only). The remaining slot (Stems
// FX) has no defined real behavior, so it renders as a visible-but-disabled
// pad rather than being hidden or invented.
Item {
    id: root

    required property string group
    required property color accentColor
    // Passed in by PadBankSection.qml based on its own real allocated
    // width -- 4 across when there's room (Deck A's side), 2 across when
    // narrow (Deck B's side), rather than this bank assuming one fixed
    // canvas size the way the old StemPads.qml did.
    required property int columns
    // Tied to which physical slot this bank is shown in (8 for the wide
    // slot next to FX, 4 for the narrow slot next to LOOP), not to which
    // bank is selected -- every bank respects whatever slot it's in. When
    // 4, only the 4 real per-stem mute pads (Vocal/Instru/Bass/Drums) show;
    // the two combo-preset pads and the two placeholders are dropped
    // entirely rather than being hidden-with-a-gap, so the remaining 4
    // reflow into a clean compact grid instead of leaving empty cells
    // where the dropped ones used to sit.
    required property int padCount
    readonly property bool showAllPads: root.padCount >= 8

    // Mixxx.PlayerManager.getPlayer() allocates a brand-new QmlPlayerProxy
    // on every call (JavaScriptOwnership, no C++ parent) -- calling it
    // inline inside another binding (as stemsModel used to) leaves that
    // instance with no persistent reference anywhere, so QML's GC reclaims
    // it right after the expression finishes, permanently killing the
    // currentTrackChanged connection the binding depended on and freezing
    // it at whatever it first read. Storing it in its own property, same
    // as every other real usage in this skin (Deck.qml, Sampler.qml,
    // WaveformOverview.qml, ...), keeps it alive so trackChanged actually
    // keeps firing.
    readonly property var player: Mixxx.PlayerManager.getPlayer(root.group)
    readonly property bool hasStems: stemCountControl.value > 0
    readonly property var stemsModel: (root.player && root.player.currentTrack)
            ? root.player.currentTrack.stemsModel : null
    // Stem file track order isn't guaranteed by the format, so stems are
    // identified by matching the loaded track's real label metadata rather
    // than assuming a fixed index.
    readonly property int vocalIndex: findStemIndex("vocal")
    readonly property int drumsIndex: findStemIndex("drum")
    readonly property int bassIndex: findStemIndex("bass")
    // Only meaningful once the other three actually resolved to real,
    // distinct rows -- otherwise (e.g. the model is still empty) this must
    // not fall back to index 0, since that would silently mis-bind
    // "Instru" to whatever row happens to be first.
    readonly property int melodyIndex: {
        if (root.vocalIndex < 0 || root.drumsIndex < 0 || root.bassIndex < 0) {
            return -1;
        }
        for (let i = 0; i < 4; i++) {
            if (i !== root.vocalIndex && i !== root.drumsIndex && i !== root.bassIndex) {
                return i;
            }
        }
        return -1;
    }

    function findStemIndex(pattern) {
        if (!root.stemsModel) {
            return -1;
        }
        // Reading stemCount (a real NOTIFY-bound property) rather than
        // calling rowCount()/get() directly is what makes this binding
        // re-evaluate once the model's data actually arrives -- plain
        // QAbstractListModel methods don't carry any QML dependency
        // tracking on their own.
        const n = root.stemsModel.stemCount;
        for (let i = 0; i < n; i++) {
            if (root.stemsModel.get(i).label.toLowerCase().includes(pattern)) {
                return i;
            }
        }
        return -1;
    }

    function stemGroup(index) {
        return index >= 0 ? (root.group.substring(0, root.group.length - 1) + "_Stem" + (index + 1) + "]") : "";
    }

    Mixxx.ControlProxy {
        id: stemCountControl

        group: root.group
        key: "stem_count"
    }
    // Separate proxies rather than reusing the pads' own internal
    // ControlProxy, so their highlight/opacity bindings below can read each
    // stem's mute state independently.
    Mixxx.ControlProxy {
        id: vocalMuteControl

        group: root.stemGroup(root.vocalIndex)
        key: "mute"
    }
    Mixxx.ControlProxy {
        id: drumsMuteControl

        group: root.stemGroup(root.drumsIndex)
        key: "mute"
    }
    Mixxx.ControlProxy {
        id: bassMuteControl

        group: root.stemGroup(root.bassIndex)
        key: "mute"
    }
    Mixxx.ControlProxy {
        id: melodyMuteControl

        group: root.stemGroup(root.melodyIndex)
        key: "mute"
    }
    // M8: the Acapella/Instrumental combo actions are real, addressable
    // ControlObjects (registered in EngineDeck), not just a QML onClicked
    // script -- so a controller mapping has something to bind a pad to.
    // Mouse clicks below go through the same trigger, keeping both input
    // paths identical.
    Mixxx.ControlProxy {
        id: stemAcapellaControl

        group: root.group
        key: "stem_acapella"
    }
    Mixxx.ControlProxy {
        id: stemInstrumentalControl

        group: root.group
        key: "stem_instrumental"
    }

    // Briefly shown on the Prepare Stems pad itself in place of its normal
    // label when a job fails (e.g. no model path configured) -- there's no
    // toast/notification system in this skin to show it in instead.
    property string stemSeparationError: ""

    // Mixxx.StemSeparation is always a registered QML singleton (see
    // qmlstemseparationproxy.cpp); when the app wasn't built with
    // AI_STEM_SEPARATION it just stays permanently idle (isRunning is
    // always false, prepareStems() is a no-op) rather than being
    // undefined, so no skin-side feature flag is needed here.
    Connections {
        target: Mixxx.StemSeparation

        function onFailed(message) {
            console.warn("Stem separation failed:", message);
            root.stemSeparationError = message;
            stemSeparationErrorTimer.restart();
        }
    }
    Timer {
        id: stemSeparationErrorTimer

        interval: 4000
        onTriggered: root.stemSeparationError = ""
    }

    // Pad grid: reading order Vocal/Instru/Bass/(Acap)/Drums/Prep/FX/(Inst)
    // -- reflows 4-wide/2-rows or 2-wide/4-rows depending on root.columns,
    // preserving that same reading order either way (row-major, matching
    // GridLayout's own default flow direction).
    GridLayout {
        anchors.fill: parent
        columns: root.columns
        columnSpacing: 6
        rowSpacing: 6

        Skin.ControlButton {
            Layout.column: 0 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(0 / root.columns)
            activeColor: "#3FA66B"
            enabled: root.hasStems && root.vocalIndex >= 0
            group: root.stemGroup(root.vocalIndex)
            // Three visual states, not just on/off: no stems yet (disabled,
            // dark, opacity 0.35); stems ready but muted (still tinted
            // with activeColor via highlight, just dimmed to read as
            // "available, currently silent"); stems ready and audible
            // (full brightness). The "mute" CO is 1 when muted, so both
            // highlight and the muted-vs-active opacity split are its
            // inverse.
            highlight: enabled && !vocalMuteControl.value
            key: "mute"
            opacity: !enabled ? 0.35 : (vocalMuteControl.value ? 0.6 : 1.0)
            text: "Vocal"
            toggleable: true
        }
        Skin.ControlButton {
            Layout.column: 1 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(1 / root.columns)
            activeColor: "#3C7993"
            enabled: root.hasStems && root.melodyIndex >= 0
            group: root.stemGroup(root.melodyIndex)
            highlight: enabled && !melodyMuteControl.value
            key: "mute"
            opacity: !enabled ? 0.35 : (melodyMuteControl.value ? 0.6 : 1.0)
            text: "Instru"
            toggleable: true
        }
        Skin.ControlButton {
            Layout.column: 2 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(2 / root.columns)
            activeColor: "#B4453F"
            enabled: root.hasStems && root.bassIndex >= 0
            group: root.stemGroup(root.bassIndex)
            highlight: enabled && !bassMuteControl.value
            key: "mute"
            opacity: !enabled ? 0.35 : (bassMuteControl.value ? 0.6 : 1.0)
            text: "Bass"
            toggleable: true
        }
        Skin.Button {
            Layout.column: 3 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(3 / root.columns)
            activeColor: root.accentColor
            enabled: root.hasStems
            opacity: enabled ? 1.0 : 0.35
            text: "(Acap)"
            visible: root.showAllPads

            onClicked: stemAcapellaControl.trigger()
        }
        Skin.ControlButton {
            // The only one of these 8 whose position actually differs
            // between modes: 5th item overall (index 4) when all 8 show,
            // but the 4th (compact index 3) once the 2 combo pads and 2
            // placeholders are dropped -- Vocal/Instru/Bass ahead of it
            // keep the same position either way.
            Layout.column: (root.showAllPads ? 4 : 3) % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor((root.showAllPads ? 4 : 3) / root.columns)
            activeColor: root.accentColor
            enabled: root.hasStems && root.drumsIndex >= 0
            group: root.stemGroup(root.drumsIndex)
            highlight: enabled && !drumsMuteControl.value
            key: "mute"
            opacity: !enabled ? 0.35 : (drumsMuteControl.value ? 0.6 : 1.0)
            text: "Drums"
            toggleable: true
        }
        // No real per-hit isolation exists for a HiHat stem -- this slot
        // instead triggers AI stem separation for tracks that don't have
        // real stems yet.
        Skin.Button {
            Layout.column: 5 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(5 / root.columns)
            activeColor: root.stemSeparationError ? "#B4453F" : root.accentColor
            enabled: !root.hasStems && !Mixxx.StemSeparation.isRunning
            opacity: enabled ? 1.0 : 0.35
            text: {
                if (Mixxx.StemSeparation.isRunning) {
                    return Math.round(Mixxx.StemSeparation.progress * 100) + "%";
                }
                return root.stemSeparationError ? "Failed" : "Prep";
            }
            visible: root.showAllPads

            ToolTip.text: root.stemSeparationError
            ToolTip.visible: root.stemSeparationError !== "" && hovered

            onClicked: {
                root.stemSeparationError = "";
                const player = Mixxx.PlayerManager.getPlayer(root.group);
                if (player && player.currentTrack) {
                    Mixxx.StemSeparation.prepareStems(player.currentTrack, root.group);
                }
            }
        }
        // Stems FX: no per-stem effect UI wired up yet -- a still-disabled
        // placeholder (deferred: wiring to Mixxx's existing per-stem
        // QuickEffectRack1 controls).
        Skin.Button {
            Layout.column: 6 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(6 / root.columns)
            enabled: false
            opacity: 0.35
            text: "FX"
            visible: root.showAllPads
        }
        Skin.Button {
            Layout.column: 7 % root.columns
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.row: Math.floor(7 / root.columns)
            activeColor: root.accentColor
            enabled: root.hasStems
            opacity: enabled ? 1.0 : 0.35
            text: "(Inst)"
            visible: root.showAllPads

            onClicked: stemInstrumentalControl.trigger()
        }
    }
}
