import QtQuick 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin

// M4: stem pads, bound to Mixxx's real native STEM playback (2.6+,
// pre-separated 4-track STEM files: Vocals/Drums/Bass/Melody-Other). The
// reference screenshot's Kick/HiHat pads aren't real -- no stem model
// isolates individual drum hits from within a mix, only a combined Drums
// stem exists -- so this grid is relabeled to the 4 real stems
// (Vocal/Instru/Bass/Drums) plus two one-press combo pads built from real
// per-stem mutes (Acapella = mute all but vocal; Instrumental = mute
// vocal only). The reference's remaining two slots ("HiHat", "Stems FX")
// have no defined real behavior, so they render as disabled placeholders
// rather than inventing scope.
Item {
    id: root

    required property string group
    required property color accentColor

    readonly property bool hasStems: stemCountControl.value > 0
    readonly property var stemsModel: {
        const player = Mixxx.PlayerManager.getPlayer(root.group);
        return (player && player.currentTrack) ? player.currentTrack.stemsModel : null;
    }
    // Stem file track order isn't guaranteed by the format, so stems are
    // identified by matching the loaded track's real label metadata
    // rather than assuming a fixed index.
    readonly property int vocalIndex: findStemIndex("vocal")
    readonly property int drumsIndex: findStemIndex("drum")
    readonly property int bassIndex: findStemIndex("bass")
    readonly property int melodyIndex: {
        for (let i = 0; i < 4; i++) {
            if (i !== root.vocalIndex && i !== root.drumsIndex && i !== root.bassIndex) {
                return i;
            }
        }
        return -1;
    }

    implicitHeight: grid.implicitHeight

    function findStemIndex(pattern) {
        if (!root.stemsModel) {
            return -1;
        }
        for (let i = 0; i < root.stemsModel.rowCount(); i++) {
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
    // ControlProxy, so the combo pads below can write multiple stems'
    // mute state in one action.
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

    GridLayout {
        id: grid

        anchors.fill: parent
        columnSpacing: 4
        columns: 4
        rowSpacing: 4
        rows: 2

        Skin.ControlButton {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.hasStems && root.vocalIndex >= 0
            group: root.stemGroup(root.vocalIndex)
            key: "mute"
            opacity: enabled ? 1.0 : 0.35
            text: "Vocal"
            toggleable: true
        }
        Skin.ControlButton {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.hasStems && root.melodyIndex >= 0
            group: root.stemGroup(root.melodyIndex)
            key: "mute"
            opacity: enabled ? 1.0 : 0.35
            text: "Instru"
            toggleable: true
        }
        Skin.ControlButton {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.hasStems && root.bassIndex >= 0
            group: root.stemGroup(root.bassIndex)
            key: "mute"
            opacity: enabled ? 1.0 : 0.35
            text: "Bass"
            toggleable: true
        }
        Skin.Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.hasStems
            opacity: enabled ? 1.0 : 0.35
            text: "Acapella"

            onClicked: {
                vocalMuteControl.value = 0;
                drumsMuteControl.value = 1;
                bassMuteControl.value = 1;
                melodyMuteControl.value = 1;
            }
        }
        Skin.ControlButton {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.hasStems && root.drumsIndex >= 0
            group: root.stemGroup(root.drumsIndex)
            key: "mute"
            opacity: enabled ? 1.0 : 0.35
            text: "Drums"
            toggleable: true
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
        Item {
            Layout.fillHeight: true
            Layout.fillWidth: true
        }
        Skin.Button {
            Layout.fillHeight: true
            Layout.fillWidth: true
            activeColor: root.accentColor
            enabled: root.hasStems
            opacity: enabled ? 1.0 : 0.35
            text: "Instrumental"

            onClicked: {
                vocalMuteControl.value = 1;
                drumsMuteControl.value = 0;
                bassMuteControl.value = 0;
                melodyMuteControl.value = 0;
            }
        }
    }
}
