import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// M16 follow-up: QML counterpart of the legacy
// DlgPrefAiLyricTranscription page, so the whisper.cpp model path is
// reachable from the skin's own gear-icon Settings popup rather than
// only the legacy Qt Widgets dialog. Both edit the exact same
// [AiLyricTranscription]/ModelPath config value (via
// WhisperTranscriptionManager::modelPath/setModelPath), so a change made
// in either surface is immediately visible in the other -- deliberately
// no duplicate storage of the key here.
//
// Layout follows the anchored-Item { ColumnLayout { RowLayout { Text +
// control } } } pattern established in Interface.qml -- a bare
// Mixxx.SettingParameter draws and lays out nothing on its own (it only
// registers a search-index entry), so every visible row needs real
// Layout parents, and the top-level container needs explicit
// left/right anchors to have a definite width.
Category {
    id: root

    label: "AI Lyrics"

    // Explicit left/right/top anchoring rather than anchors.fill or a
    // ScrollView: this is the pattern the already-working pages use
    // (see Interface.qml). A ScrollView here left the ColumnLayout with
    // a circular width dependency (its width came from the ScrollView's
    // availableWidth, whose contentWidth came back from the content),
    // so no row ever got a definite width, wrapMode never engaged, and
    // every row ran off the right edge of the panel. These pages have
    // few rows, so scrolling isn't needed anyway.
    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.bottom: parent.bottom

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 8

            Text {
                Layout.fillWidth: true
                color: Theme.white
                font.pixelSize: 14
                text: "Local AI transcription generates synced lyrics for tracks that have none online. It runs entirely on this machine using a whisper.cpp GGML model."
                wrapMode: Text.WordWrap
            }
            // Built without AI_LYRIC_TRANSCRIPTION: the singleton still
            // exists (always registered, see qmllyrictranscriptionproxy.cpp)
            // but has no manager behind it, so say so instead of offering
            // controls that would silently do nothing.
            Text {
                Layout.fillWidth: true
                color: Theme.red
                font.pixelSize: 13
                text: "This build was compiled without AI lyric transcription support."
                visible: !Mixxx.LyricTranscription.available
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: Mixxx.LyricTranscription.available

                Text {
                    Layout.fillWidth: true
                    color: Theme.white
                    font.pixelSize: 14
                    text: "Whisper model file"

                    Mixxx.SettingParameter {
                        label: "Whisper model file"
                    }
                }
                TextField {
                    id: modelPathField

                    Layout.preferredWidth: 320
                    placeholderText: "Path to a .bin GGML model"
                    text: Mixxx.LyricTranscription.modelPath

                    onEditingFinished: Mixxx.LyricTranscription.modelPath = text
                }
                Skin.Button {
                    text: "Browse…"

                    onClicked: modelFileDialog.open()
                }
            }
            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    color: Theme.white
                    font.pixelSize: 14
                    text: "Model status"
                }
                Text {
                    color: Mixxx.LyricTranscription.modelStatus === "Found" ? Theme.white : Theme.red
                    font.bold: true
                    font.pixelSize: 14
                    text: Mixxx.LyricTranscription.modelStatus
                }
            }
            Text {
                Layout.fillWidth: true
                color: Theme.midGray
                font.pixelSize: 12
                text: "Tip: lyrics are looked up on lrclib.net first, which is free and instant. The AI model is only used as a fallback when no online match exists."
                wrapMode: Text.WordWrap
            }
        }
    }

    FileDialog {
        id: modelFileDialog

        nameFilters: ["GGML model (*.bin)", "All files (*)"]
        title: qsTr("Select Whisper GGML Model")

        onAccepted: {
            Mixxx.LyricTranscription.setModelPathFromUrl(selectedFile);
            modelPathField.text = Mixxx.LyricTranscription.modelPath;
        }
    }
}
