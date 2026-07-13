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
            runOrConfirm(() => player.loadTrackFromLocationUrl(url));
            drop.accepted = true;
            return ;
        }
    }
}
