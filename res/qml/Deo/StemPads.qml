import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
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

    implicitHeight: mainColumn.implicitHeight

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
    // A real dropdown has nothing useful to open with only one entry --
    // this is a visual placeholder for pad-bank switching (hotcues,
    // sampler, custom banks), deferred out of M4's scope.
    ListModel {
        id: bankOnlyModel

        ListElement {
            display: "STEMS 2.0"
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 4

        // Rotated "PADS" label on the left edge, matching the FX/LOOP
        // vertical labels elsewhere in the deck.
        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: 14

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
        ColumnLayout {
            id: mainColumn

            Layout.fillHeight: true
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 18
                spacing: 2

                Skin.ComboBox {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    currentIndex: 0
                    model: bankOnlyModel
                    textRole: "display"
                }
                Skin.Button {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 18
                    enabled: false
                    text: "◀"
                }
                Skin.Button {
                    Layout.fillHeight: true
                    Layout.preferredWidth: 18
                    enabled: false
                    text: "▶"
                }
            }
            GridLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                columnSpacing: 3
                columns: 4
                rowSpacing: 3
                rows: 2

                Skin.ControlButton {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    activeColor: "#3FA66B"
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
                    activeColor: "#3C7993"
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
                    activeColor: "#B4453F"
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
                    text: "(Acapella)"

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
                // No real per-hit isolation exists for these two --
                // visible so the grid always shows 8 pads, but disabled
                // since there's nothing real to bind them to yet.
                Skin.Button {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    enabled: false
                    opacity: 0.35
                    text: "HiHat"
                }
                Skin.Button {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    enabled: false
                    opacity: 0.35
                    text: "Stems FX"
                }
                Skin.Button {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    activeColor: root.accentColor
                    enabled: root.hasStems
                    opacity: enabled ? 1.0 : 0.35
                    text: "(Instrument)"

                    onClicked: {
                        vocalMuteControl.value = 1;
                        drumsMuteControl.value = 0;
                        bassMuteControl.value = 0;
                        melodyMuteControl.value = 0;
                    }
                }
            }
        }
    }
}
