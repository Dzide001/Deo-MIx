import QtQuick 2.12
import QtQuick.Controls
import Mixxx 1.0 as Mixxx
import "." as Deo
import ".." as Skin
import "../Theme"

// M5: AUDIO/VIDEO/MASTER tab shell (SCRATCH removed entirely — not a
// defined feature, deprioritized per discussion). AUDIO and MASTER have
// full content; VIDEO now has a real (Stage 3b) enable toggle wired to
// mixxx::VideoEngineManager's real, controller-mappable "[VideoEngine]"/
// "enabled" ControlObject -- see milestone_12_video_spec_addendum.md.
// Actual deck video layers/live preview are Stage 3c+, not yet here.
//
// Rebuilt as a plain Item per the user's reference mockup
// (MixerDesignMockupA.qml): a fixed 297x378 authored canvas (this
// panel's own real allocated size, confirmed via debug logging), scaled
// to whatever width/height the Layout above actually allocates -- same
// fixed-wrapper + Scale technique used for every other section ported
// from a mockup this session, so the literal mockup coordinates below
// don't need per-instance percentage math.
//
// currentTab replaces the previous StackLayout, keeping every tab's
// contents alive (not recreated via a Loader) so switching away and
// back doesn't lose control state (acceptance criterion #6).
//
// The crossfader is persistent across tabs rather than living inside
// AudioMixerPanel: both deo_master_panel_spec.json and
// "DEo audio_mixer_panel_spec.json" show a fader in the identical
// position/style at the bottom of the mixer regardless of which tab is
// selected, matching how a physical mixer's crossfader doesn't change
// based on what's shown on a screen above it.
Item {
    id: root

    required property color accentColorA
    required property color accentColorB
    property int currentTab: 0

    clip: true

    Item {
        id: canvas

        width: 297
        height: 378

        transform: Scale {
            origin.x: 0
            origin.y: 0
            xScale: root.width / canvas.width
            yScale: root.height / canvas.height
        }

        Skin.Button {
            x: 0
            y: 0
            width: 99
            height: 33
            checkable: true
            checked: root.currentTab === 0
            text: "AUDIO"

            onClicked: root.currentTab = 0
        }
        Skin.Button {
            x: 99
            y: 0
            width: 99
            height: 33
            checkable: true
            checked: root.currentTab === 1
            text: "VIDEO"

            onClicked: root.currentTab = 1
        }
        Skin.Button {
            x: 198
            y: 0
            width: 99
            height: 33
            checkable: true
            checked: root.currentTab === 2
            text: "MASTER"

            onClicked: root.currentTab = 2
        }

        Deo.AudioMixerPanel {
            x: 0
            y: 33
            accentColorA: root.accentColorA
            accentColorB: root.accentColorB
            visible: root.currentTab === 0
        }
        Column {
            x: 0
            y: 33
            width: 297
            height: 345
            spacing: 10
            visible: root.currentTab === 1

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.deckTextSecondary
                font.family: Theme.fontFamily
                font.pixelSize: 11
                text: Mixxx.VideoEngine.available
                    ? "Video engine available"
                    : "Video engine not available in this build"
            }
            Skin.ControlButton {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 120
                height: 26
                activeColor: Theme.accentColor
                enabled: Mixxx.VideoEngine.available
                group: "[VideoEngine]"
                key: "enabled"
                opacity: enabled ? 1.0 : 0.35
                text: "ENGINE"
                toggleable: true
            }
            // Stage 3e: the live preview itself moved out of this tab
            // entirely -- it's now Deo.VideoPreviewPanel, a floating
            // panel added at the window root (main.qml), matching
            // VirtualDJ's pattern instead of an inline box that resized
            // this column whenever it appeared (the actual cause of the
            // Stage 3c/3d-3 layout-shift bug). See
            // milestone_12_video_spec_addendum.md.
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.deckTextSecondary
                font.family: Theme.fontFamily
                font.italic: true
                font.pixelSize: 10
                text: "Live preview is the floating panel above the waveform"
                visible: Mixxx.VideoEngine.available
            }
            // M12 Stage 3y: camera is a plain method call
            // (setCameraSource()), not a real ControlObject -- unlike
            // ENGINE above, so a checkable Skin.Button (not
            // Skin.ControlButton) is the right fit here, same reasoning
            // as TopBar.qml's settings gear. Defaults to the system's
            // first camera; a device picker for multi-camera machines is
            // a smaller follow-up, not built here.
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Skin.Button {
                    id: cameraAToggle

                    width: 56
                    height: 26
                    checkable: true
                    enabled: Mixxx.VideoEngine.available
                    opacity: enabled ? 1.0 : 0.35
                    text: "CAM A"

                    onClicked: Mixxx.VideoEngine.setCameraSource("[Channel1]", checked)
                }
                Skin.Button {
                    id: cameraBToggle

                    width: 56
                    height: 26
                    checkable: true
                    enabled: Mixxx.VideoEngine.available
                    opacity: enabled ? 1.0 : 0.35
                    text: "CAM B"

                    onClicked: Mixxx.VideoEngine.setCameraSource("[Channel2]", checked)
                }
            }
            // M12 Stage 6: NDI output is a plain method call
            // (enableNdiOutput()), same reasoning as camera above -- not a
            // real ControlObject. ndiOutputAvailable is a compile-time
            // constant (was this build actually compiled with
            // -DVIDEO_ENGINE_NDI_OUTPUT=ON), so this whole row is simply
            // absent rather than shown-but-disabled when the build doesn't
            // support it -- there's nothing a user could do about a
            // missing SDK from inside the running app.
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8
                visible: Mixxx.VideoEngine.available && Mixxx.VideoEngine.ndiOutputAvailable

                Skin.Button {
                    id: ndiOutputToggle

                    width: 56
                    height: 26
                    checkable: true
                    text: "NDI"

                    onClicked: Mixxx.VideoEngine.enableNdiOutput(checked, ndiSourceNameField.text)
                }
                Skin.TextField {
                    id: ndiSourceNameField

                    width: 140
                    height: 26
                    text: "Deo Pro DJ"
                    placeholderText: "NDI source name"
                }
            }
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.deckTextSecondary
                font.family: Theme.fontFamily
                font.italic: true
                font.pixelSize: 10
                text: "Free layer position/opacity/z-order are still in progress"
                horizontalAlignment: Text.AlignHCenter
                visible: Mixxx.VideoEngine.available
            }
        }
        Deo.MasterPanel {
            x: 0
            y: 33
            width: 297
            height: 345
            visible: root.currentTab === 2
        }

        Deo.Crossfader {
            x: 34
            y: 340
            width: 229
            height: 30
        }
    }
}
