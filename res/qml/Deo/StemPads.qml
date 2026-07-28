import QtQuick 2.12
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// M4: stem pads, bound to Mixxx's real native STEM playback (2.6+,
// pre-separated 4-track STEM files: Vocals/Drums/Bass/Melody-Other).
// Sized/positioned per deckA_pads_block in Deo Pro dj_layout_spec.json:
// a PADS vertical label + STEMS 2.0 bank-switcher header (20% height),
// then two 4-pad rows (40% height each). The reference screenshot's
// Kick/HiHat pads aren't real -- no stem model isolates individual drum
// hits from within a mix, only a combined Drums stem exists -- so this
// grid is relabeled to the 4 real stems (Vocal/Instru/Bass/Drums) plus
// two one-press combo pads built from real per-stem mutes (Acapella =
// mute all but vocal; Instrumental = mute vocal only). The remaining two
// slots (HiHat, Stems FX) have no defined real behavior, so they render
// as visible-but-disabled pads rather than being hidden or invented.
Item {
    id: root

    required property string group
    required property color accentColor

    // Fixed authored canvas, matching PadsDesignMockup.qml exactly --
    // DeckPanel.qml scales this to fit the real allocated box (a Scale
    // transform, not anchors.fill) rather than assuming the real box
    // already equals 252x108.
    height: 108
    width: 252

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
    // identified by matching the loaded track's real label metadata
    // rather than assuming a fixed index.
    readonly property int vocalIndex: findStemIndex("vocal")
    readonly property int drumsIndex: findStemIndex("drum")
    readonly property int bassIndex: findStemIndex("bass")
    // Only meaningful once the other three actually resolved to real,
    // distinct rows -- otherwise (e.g. the model is still empty) this
    // must not fall back to index 0, since that would silently mis-bind
    // "Instru" to whatever row happens to be first (see melodyIndex's
    // prior fallback bug).
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

    // Geometry, taken directly from the user's reference mockup
    // (PadsDesignMockup.qml), which was drawn at DA lower half's real
    // allocated size, 252x108.
    readonly property real labelX: 8
    readonly property real labelWidth: 18
    readonly property real comboX: 32
    readonly property real comboY: 15
    readonly property real comboWidth: 102
    readonly property real comboHeight: 17
    readonly property real padRow1Y: 38
    readonly property real padRow2Y: 69
    readonly property real padHeight: 25
    readonly property real padWidth: 48
    readonly property real padCol1X: 32
    readonly property real padCol2X: 86
    readonly property real padCol3X: 140
    readonly property real padCol4X: 194

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
    // M8: the Acapella/Instrumental combo actions are now real, addressable
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
    // A real dropdown has nothing useful to open with only one entry --
    // this is a visual placeholder for pad-bank switching (hotcues,
    // sampler, custom banks), deferred out of M4's scope.
    ListModel {
        id: bankOnlyModel

        ListElement {
            display: "Stems"
        }
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

    // Rotated "PADS" label on the left edge, matching the FX/LOOP vertical
    // labels elsewhere in the deck.
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
            text: "PADS"
        }
    }
    // Bank-switcher header -- a real dropdown with only one entry has
    // nothing useful to open; this is a visual placeholder for pad-bank
    // switching (hotcues, sampler, custom banks), deferred out of M4's
    // scope. The previous separate prev/next arrow buttons were dropped
    // (always non-functional placeholders) in favor of the ComboBox's own
    // built-in dropdown indicator, matching the reference mockup.
    Skin.ComboBox {
        // font.pixelSize: 9, not 8.5 -- Qt's font.pixelSize is an
        // int-typed property; the Button.qml alias used for the pad
        // labels crashed the whole app on exactly this ("Invalid property
        // assignment: int expected") when given a decimal. 10 (the deck's
        // baseline text size elsewhere) * 0.85 = 8.5, rounded to 9.
        //
        // +8/-4 on size/position -- Skin.ComboBox's background Rectangle
        // insets itself 4px from the control's actual box (see
        // res/qml/ComboBox.qml), so its visible dark pill renders smaller
        // than its declared width/height. This compensates so the
        // VISIBLE box lands exactly on the mockup's coordinates.
        currentIndex: 0
        font.pixelSize: 9
        height: root.comboHeight + 8
        model: bankOnlyModel
        textRole: "display"
        width: root.comboWidth + 8
        x: root.comboX - 4
        y: root.comboY - 4
    }
    // Pad grid: row 1 (Vocal/Instru/Bass/Acap), row 2 (Drums/Prep/FX/Inst).
    Skin.ControlButton {
        activeColor: "#3FA66B"
        enabled: root.hasStems && root.vocalIndex >= 0
        group: root.stemGroup(root.vocalIndex)
        // Three visual states, not just on/off: no stems yet (disabled,
        // dark, opacity 0.35); stems ready but muted (still tinted with
        // activeColor via highlight, just dimmed to read as "available,
        // currently silent"); stems ready and audible (full brightness).
        // The "mute" CO is 1 when muted, so both highlight and the
        // muted-vs-active opacity split are its inverse.
        highlight: enabled && !vocalMuteControl.value
        height: root.padHeight
        key: "mute"
        opacity: !enabled ? 0.35 : (vocalMuteControl.value ? 0.6 : 1.0)
        text: "Vocal"
        toggleable: true
        width: root.padWidth
        x: root.padCol1X
        y: root.padRow1Y
    }
    Skin.ControlButton {
        activeColor: "#3C7993"
        enabled: root.hasStems && root.melodyIndex >= 0
        group: root.stemGroup(root.melodyIndex)
        highlight: enabled && !melodyMuteControl.value
        height: root.padHeight
        key: "mute"
        opacity: !enabled ? 0.35 : (melodyMuteControl.value ? 0.6 : 1.0)
        text: "Instru"
        toggleable: true
        width: root.padWidth
        x: root.padCol2X
        y: root.padRow1Y
    }
    Skin.ControlButton {
        activeColor: "#B4453F"
        enabled: root.hasStems && root.bassIndex >= 0
        group: root.stemGroup(root.bassIndex)
        highlight: enabled && !bassMuteControl.value
        height: root.padHeight
        key: "mute"
        opacity: !enabled ? 0.35 : (bassMuteControl.value ? 0.6 : 1.0)
        text: "Bass"
        toggleable: true
        width: root.padWidth
        x: root.padCol3X
        y: root.padRow1Y
    }
    Skin.Button {
        activeColor: root.accentColor
        enabled: root.hasStems
        height: root.padHeight
        opacity: enabled ? 1.0 : 0.35
        text: "(Acap)"
        width: root.padWidth
        x: root.padCol4X
        y: root.padRow1Y

        onClicked: stemAcapellaControl.trigger()
    }
    Skin.ControlButton {
        activeColor: root.accentColor
        enabled: root.hasStems && root.drumsIndex >= 0
        group: root.stemGroup(root.drumsIndex)
        highlight: enabled && !drumsMuteControl.value
        height: root.padHeight
        key: "mute"
        opacity: !enabled ? 0.35 : (drumsMuteControl.value ? 0.6 : 1.0)
        text: "Drums"
        toggleable: true
        width: root.padWidth
        x: root.padCol1X
        y: root.padRow2Y
    }
    // No real per-hit isolation exists for a HiHat stem -- this slot
    // instead triggers AI stem separation for tracks that don't have real
    // stems yet. Stems FX is a separate, still-disabled placeholder
    // (deferred: wiring to Mixxx's existing per-stem QuickEffectRack1
    // controls).
    Skin.Button {
        activeColor: root.stemSeparationError ? "#B4453F" : root.accentColor
        enabled: !root.hasStems && !Mixxx.StemSeparation.isRunning
        height: root.padHeight
        opacity: enabled ? 1.0 : 0.35
        text: {
            if (Mixxx.StemSeparation.isRunning) {
                return Math.round(Mixxx.StemSeparation.progress * 100) + "%";
            }
            return root.stemSeparationError ? "Failed" : "Prep";
        }
        width: root.padWidth
        x: root.padCol2X
        y: root.padRow2Y

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
    Skin.Button {
        enabled: false
        height: root.padHeight
        opacity: 0.35
        text: "FX"
        width: root.padWidth
        x: root.padCol3X
        y: root.padRow2Y
    }
    Skin.Button {
        activeColor: root.accentColor
        enabled: root.hasStems
        height: root.padHeight
        opacity: enabled ? 1.0 : 0.35
        text: "(Inst)"
        width: root.padWidth
        x: root.padCol4X
        y: root.padRow2Y

        onClicked: stemInstrumentalControl.trigger()
    }
}
