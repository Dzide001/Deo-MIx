import Mixxx 1.0 as Mixxx
import QtQuick 2.12
import QtQuick.Controls
import QtQuick.Layouts

// Handles drops on decks and samplers
DropArea {
    id: root

    required property string group
    property var player: Mixxx.PlayerManager.getPlayer(group)
    property var pendingAction: null

    Mixxx.ControlProxy {
        id: playControl

        group: root.group
        key: "play"
    }

    // M7: dropping a track onto a deck that's already playing needs a
    // deliberate choice, not a silent cut -- confirm before replacing.
    // Applies to both file/library drops and deck-to-deck clone drops,
    // since both replace whatever's currently loaded and playing.
    Popup {
        id: confirmReplaceDialog

        focus: true
        modal: true
        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        ColumnLayout {
            Label {
                text: qsTr("This deck is playing. Replace the loaded track?")
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight

                Button {
                    text: qsTr("Cancel")

                    onClicked: {
                        root.pendingAction = null;
                        confirmReplaceDialog.close();
                    }
                }
                Button {
                    text: qsTr("Replace")

                    onClicked: {
                        if (root.pendingAction) {
                            root.pendingAction();
                        }
                        root.pendingAction = null;
                        confirmReplaceDialog.close();
                    }
                }
            }
        }
    }

    function runOrConfirm(action) {
        if (playControl.value > 0) {
            root.pendingAction = action;
            confirmReplaceDialog.open();
        } else {
            action();
        }
    }

    // M12 Stage 3f/3h: video files load onto the deck through this exact
    // same drop target, not a separate video-only picker -- matching the
    // user's explicit ask ("I should be able to load onto the deck, I
    // don't need a special one for videos") and how VirtualDJ's own
    // browser/deck treats video files as just another loadable file type
    // (its own reference screenshot shows a loaded music video with a
    // real waveform, elapsed/remain time, and BPM/key -- the same file is
    // simultaneously the deck's audio track and its video source).
    //
    // A dropped video file therefore loads through BOTH paths: the normal
    // player.loadTrackFromLocationUrl() (Mixxx's existing
    // SoundSourceFFmpeg backend already recognizes mp4/m4v/mov and
    // extracts the embedded audio track -- real waveform, BPM, playback,
    // no new code needed there) AND Mixxx.VideoEngine.loadVideo() for the
    // video rendering side. Stage 3f originally routed video files AWAY
    // from the audio load entirely (an "independent video clip,
    // decoupled from the audio track" reading of the earlier VJ-clip-deck
    // decision) -- corrected here once it was clear the deck should show
    // real waveform/audio for a loaded video, not silence.
    readonly property var videoFileExtensions: ["mp4", "mov", "m4v", "mkv", "webm", "avi"]

    function isVideoFile(url) {
        const path = url.toString().toLowerCase();
        return root.videoFileExtensions.some((ext) => path.endsWith("." + ext));
    }

    onDropped: (drop) => {
        if (drop.formats.includes("mixxx/player")) {
            const sourceGroup = drop.getDataAsString("mixxx/player");
            // Prevent dropping a deck onto itself
            if (sourceGroup == root.group)
                return ;

            runOrConfirm(() => player.cloneFromGroup(sourceGroup));
            drop.accepted = true;
            return ;
        }
        if (drop.hasUrls && drop.urls.length > 0) {
            let url = drop.urls[0];
            if (root.isVideoFile(url)) {
                Mixxx.VideoEngine.loadVideo(root.group, url.toString());
            }
            runOrConfirm(() => player.loadTrackFromLocationUrl(url));
            drop.accepted = true;
            return ;
        }
    }
}
