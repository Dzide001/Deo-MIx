import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// M5 MASTER tab. Everything here binds to real, existing Mixxx COs found
// in src/engine/enginemixer.cpp, src/recording/, and src/broadcast/ — no
// new engine work needed, unlike Backspin. The master effect slot reuses
// the same Mixxx.EffectSlotProxy pattern as the per-deck FX rack (M3),
// just routed to [Master] instead of a channel, using unit 3 (of the 4
// standard units — 1 and 2 are already used by the decks).
ColumnLayout {
    id: root

    spacing: 10

    readonly property Mixxx.EffectSlotProxy masterEffectSlot: Mixxx.EffectsManager.getEffectSlot(3, 1)

    Mixxx.ControlProxy {
        id: recordingStatus

        group: "[Recording]"
        key: "status"
    }
    Mixxx.ControlProxy {
        id: recordingToggle

        group: "[Recording]"
        key: "toggle_recording"
    }
    Mixxx.ControlProxy {
        id: broadcastStatus

        group: "[Shoutcast]"
        key: "status"
    }
    Deo.CuratedEffectsModel {
        id: curatedEffects
    }
    // Pin the master effect unit to the master bus permanently.
    Mixxx.ControlProxy {
        id: masterEffectRouting

        group: "[EffectRack1_EffectUnit3]"
        key: "group_[Master]_enable"

        onInitializedChanged: {
            value = 1;
        }
    }

    RowLayout {
        id: contentRow

        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: 10

        // Master column.
        ColumnLayout {
            Layout.preferredWidth: contentRow.width * 0.20
            Layout.minimumWidth: 90
            Layout.fillHeight: true
            spacing: 6

            Skin.ControlKnob {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.accentColor
                group: "[Master]"
                height: 48
                key: "gain"
                width: 48
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.deckTextSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 10
                text: "MASTER"
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                activeColor: Theme.accentColor
                group: "[Master]"
                key: "enabled"
                text: "ON"
                toggleable: true
            }
            Skin.Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                activeColor: Theme.red
                highlight: recordingStatus.value > 0
                text: "REC"

                onClicked: recordingToggle.trigger()
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 3

                Skin.VuMeter {
                    Layout.fillHeight: true
                    group: "[Master]"
                    key: "vu_meter_left"
                    width: 5
                }
                Skin.VuMeter {
                    Layout.fillHeight: true
                    group: "[Master]"
                    key: "vu_meter_right"
                    width: 5
                }
            }
        }
        // Headphone + Master Effect + Record/Broadcast column.
        ColumnLayout {
            Layout.preferredWidth: contentRow.width * 0.60
            Layout.minimumWidth: 200
            Layout.fillHeight: true
            spacing: 10

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 16

                ColumnLayout {
                    Skin.ControlKnob {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.white
                        group: "[Master]"
                        height: 40
                        key: "headGain"
                        width: 40
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.deckTextSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        text: "VOL"
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignVCenter
                    color: Theme.deckTextSecondary
                    font.pixelSize: 20
                    text: "🎧"
                }
                ColumnLayout {
                    Skin.ControlKnob {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.white
                        group: "[Master]"
                        height: 40
                        key: "headMix"
                        width: 40
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.deckTextSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        text: "MIX"
                    }
                }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.deckTextSecondary
                font.bold: true
                font.family: Theme.fontFamily
                font.pixelSize: 11
                text: "MASTER EFFECT"
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Skin.ComboBox {
                    id: masterEffectSelector

                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    model: curatedEffects
                    textRole: "display"

                    Component.onCompleted: {
                        const rowCount = model.rowCount();
                        for (let i = 0; i < rowCount; i++) {
                            if (model.get(i).effectId === root.masterEffectSlot.effectId) {
                                currentIndex = i;
                                return;
                            }
                        }
                        currentIndex = -1;
                    }
                    onActivated: index => {
                        root.masterEffectSlot.effectId = model.get(index).effectId;
                    }
                }
                Skin.ControlButton {
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    activeColor: Theme.accentColor
                    group: root.masterEffectSlot.group
                    key: "enabled"
                    text: "⚙"
                    toggleable: true
                }
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.deckTextSecondary
                font.bold: true
                font.family: Theme.fontFamily
                font.pixelSize: 11
                text: "RECORD/BROADCAST"
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.deckPanelAltBackground
                radius: 4

                Label {
                    anchors.centerIn: parent
                    color: Theme.deckTextSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    text: (recordingStatus.value > 0 ? "Recording" : "Not recording") + (broadcastStatus.value > 0 ? " • Broadcasting" : "")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Skin.Button {
                    Layout.fillWidth: true
                    activeColor: Theme.red
                    highlight: recordingStatus.value > 0
                    text: "REC"

                    onClicked: recordingToggle.trigger()
                }
                Skin.ControlButton {
                    Layout.fillWidth: true
                    activeColor: Theme.accentColor
                    group: "[Shoutcast]"
                    key: "enabled"
                    text: "BCAST"
                    toggleable: true
                }
                Skin.Button {
                    Layout.fillWidth: true
                    text: "FILE"

                    onClicked: {
                        // Recording file path/format live in Preferences ->
                        // Recording; not duplicating a file picker here.
                        Mixxx.PreferencesDialog.show();
                    }
                }
            }
        }
        // Mic column.
        ColumnLayout {
            Layout.preferredWidth: contentRow.width * 0.20
            Layout.minimumWidth: 90
            Layout.fillHeight: true
            spacing: 6

            Skin.ControlKnob {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.gainKnobColor
                group: "[Microphone]"
                height: 48
                key: "pregain"
                width: 48
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.deckTextSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 10
                text: "MIC VOL"
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                activeColor: Theme.accentColor
                group: "[Microphone]"
                key: "talkover"
                text: "ON"
                toggleable: true
            }
            Skin.Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 26
                activeColor: Theme.red
                highlight: recordingStatus.value > 0
                text: "REC"

                onClicked: recordingToggle.trigger()
            }
            Skin.VuMeter {
                Layout.fillWidth: true
                Layout.fillHeight: true
                group: "[Microphone]"
                key: "vu_meter"
            }
        }
    }
}
