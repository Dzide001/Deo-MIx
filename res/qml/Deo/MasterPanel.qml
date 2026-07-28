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

    // 4, not 10 -- MasterPanel's real allocated width is ~297px (18% of
    // deck_section's width, same allocation the AUDIO tab already had to
    // fit into), but this row's minimums used to sum to 90+200+90=380
    // (plus spacing) -- 83px over budget, which is what was visibly
    // cutting content off. Closing gaps here is part of the same fix as
    // the column minimums/knob sizes below.
    RowLayout {
        id: contentRow

        // maximumHeight 300, not just fillHeight -- MasterPanel's real
        // canvas is 345 tall, but the crossfader (a sibling in
        // MixerTabs.qml, not part of this panel) occupies a fixed band
        // at the bottom of that same 345px, roughly local y=307-337.
        // Without a cap, fillHeight children below (the record/broadcast
        // status box, both VU meters) grew to fill the full 345 and
        // visually bled into the crossfader's own space. 300 leaves a
        // clear ~45px margin above it.
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.maximumHeight: 300
        spacing: 4

        // Master column. minimumWidth 55, not 90 -- knob shrunk to match
        // (38, not 48) so it still fits comfortably inside the smaller
        // column instead of overflowing it.
        ColumnLayout {
            Layout.preferredWidth: contentRow.width * 0.20
            Layout.minimumWidth: 55
            Layout.fillHeight: true
            spacing: 6

            Skin.ControlKnob {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.accentColor
                group: "[Master]"
                height: 38
                key: "gain"
                // M8: representative "driven by hardware" indicator --
                // lights up briefly when a mapped controller (not the
                // mouse/UI) moves this knob.
                showHardwareIndicator: true
                width: 38
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
                // M8: representative "driven by hardware" indicator, see
                // the gain knob above.
                showHardwareIndicator: true
                text: "ON"
                toggleable: true
            }
            Skin.ControlButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                activeColor: Theme.accentColor
                group: "[Master]"
                key: "limiter_enabled"
                // Rough-sketch master limiter (tanh soft-knee saturation,
                // no attack/release envelope yet) -- off by default so it
                // never changes existing sessions' sound without opt-in.
                text: "LIM"
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
        // minimumWidth 150, not 200 -- see contentRow's own comment above
        // for why the old 90/200/90 sum was overflowing this tab's real
        // ~297px allocation.
        ColumnLayout {
            Layout.preferredWidth: contentRow.width * 0.60
            Layout.minimumWidth: 150
            Layout.fillHeight: true
            spacing: 6

            // spacing 3 (not 16) and each knob/button shrunk -- the old
            // 40/40/40-wide row with 16px gaps needed ~204px on its own,
            // more than this whole column's new 150px minimum.
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 3

                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter

                    Skin.ControlKnob {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.white
                        group: "[Master]"
                        height: 32
                        key: "headGain"
                        width: 32
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
                    font.pixelSize: 16
                    text: "🎧"
                }
                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter

                    Skin.ControlKnob {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.white
                        group: "[Master]"
                        height: 32
                        key: "headMix"
                        width: 32
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.deckTextSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        text: "MIX"
                    }
                }
                // Layout.alignment: Qt.AlignVCenter -- was implicitly
                // top-aligned (unlike the two knob columns and the
                // headphone emoji beside it, which are all explicitly
                // centered), the actual cause of SPL sitting visibly
                // higher than VOL/MIX despite its own button being
                // shorter (22px) than their knobs (32px).
                ColumnLayout {
                    Layout.alignment: Qt.AlignVCenter

                    Skin.ControlButton {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredHeight: 22
                        Layout.preferredWidth: 28
                        activeColor: Theme.white
                        group: "[Master]"
                        key: "headSplit"
                        text: "SPL"
                        toggleable: true
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: Theme.deckTextSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        text: "SPLIT"
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
            // Layout.preferredHeight 40, not fillHeight -- this was the
            // one genuinely flexible element in the column, so it grew
            // to fill this whole tab's real height (345, only capped to
            // 300 by contentRow's own new maximumHeight above) despite
            // its own content (2 short status lines) needing nowhere
            // near that much room -- both the "too tall/empty" look and
            // part of the crossfader-bleed were this box unnecessarily
            // absorbing all the leftover space.
            Rectangle {
                id: recordBroadcastStatus

                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: Theme.deckPanelAltBackground
                radius: 4

                // M11: distinct visual treatment per connection state
                // (disconnected/connecting/connected/error), not just an
                // on/off light -- "[Shoutcast]"/"status" already carries
                // BroadcastProfile::StatusStates (0=unconnected,
                // 1=connecting, 2=connected, 3=failure), it just wasn't
                // being read as anything but a boolean before.
                function broadcastStatusText() {
                    switch (broadcastStatus.value) {
                    case 1:
                        return "Connecting…";
                    case 2:
                        return "Broadcasting";
                    case 3:
                        return "Broadcast Error";
                    default:
                        return "";
                    }
                }
                function broadcastStatusColor() {
                    switch (broadcastStatus.value) {
                    case 1:
                        return "#D9A441";
                    case 2:
                        return Theme.accentColor;
                    case 3:
                        return Theme.red;
                    default:
                        return Theme.deckTextSecondary;
                    }
                }

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    Label {
                        color: Theme.deckTextSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        // M10: elapsed time (MM:SS) alongside the status
                        // text while recording, per the milestone's
                        // acceptance criteria -- Mixxx.Recording.durationText
                        // already existed in C++
                        // (RecordingManager::durationRecorded) but was never
                        // bound anywhere in this skin.
                        text: recordingStatus.value > 0 ? "Recording • " + Mixxx.Recording.durationText : "Not recording"
                    }
                    Rectangle {
                        Layout.preferredHeight: 6
                        Layout.preferredWidth: 6
                        color: recordBroadcastStatus.broadcastStatusColor()
                        radius: 3
                        visible: broadcastStatus.value > 0
                    }
                    Label {
                        color: recordBroadcastStatus.broadcastStatusColor()
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        text: recordBroadcastStatus.broadcastStatusText()
                        visible: broadcastStatus.value > 0
                    }
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
        // Mic column. minimumWidth 55, not 90 -- knob shrunk to match (38,
        // not 48), same treatment as the Master column.
        ColumnLayout {
            Layout.preferredWidth: contentRow.width * 0.20
            Layout.minimumWidth: 55
            Layout.fillHeight: true
            spacing: 6

            Skin.ControlKnob {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.gainKnobColor
                group: "[Microphone]"
                height: 38
                key: "pregain"
                width: 38
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
