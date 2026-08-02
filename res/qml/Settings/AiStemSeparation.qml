import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// QML counterpart of the legacy DlgPrefAiStemSeparation page, so the
// Demucs ONNX model path is reachable from the skin's own gear-icon
// Settings popup rather than only the legacy Qt Widgets dialog. Both
// edit the exact same [AiStemSeparation]/ModelPath config value (via
// StemSeparationManager::modelPath/setModelPath) -- deliberately no
// duplicate storage of the key here. See AiLyricTranscription.qml for
// the layout-pattern rationale; this page mirrors it.
Category {
    id: root

    label: "AI Stems"

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
                text: "AI stem separation splits a track into vocal, drum, bass and instrumental stems on this machine, using a Demucs ONNX model. Separated stems drive the deck Stems pads."
                wrapMode: Text.WordWrap
            }
            Text {
                Layout.fillWidth: true
                color: Theme.red
                font.pixelSize: 13
                text: "This build was compiled without AI stem separation support."
                visible: !Mixxx.StemSeparation.available
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: Mixxx.StemSeparation.available

                Text {
                    Layout.fillWidth: true
                    color: Theme.white
                    font.pixelSize: 14
                    text: "Demucs model file"

                    Mixxx.SettingParameter {
                        label: "Demucs model file"
                    }
                }
                TextField {
                    id: modelPathField

                    Layout.preferredWidth: 320
                    placeholderText: "Path to a Demucs .onnx model"
                    text: Mixxx.StemSeparation.modelPath

                    onEditingFinished: Mixxx.StemSeparation.modelPath = text
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
                    color: Mixxx.StemSeparation.modelStatus === "Found" ? Theme.white : Theme.red
                    font.bold: true
                    font.pixelSize: 14
                    text: Mixxx.StemSeparation.modelStatus
                }
            }
            Text {
                Layout.fillWidth: true
                color: Theme.midGray
                font.pixelSize: 12
                text: "Separation is CPU/GPU intensive and runs in the background; a track only needs separating once, after which its stems are reused."
                wrapMode: Text.WordWrap
            }
        }
    }

    FileDialog {
        id: modelFileDialog

        nameFilters: ["ONNX model (*.onnx)", "All files (*)"]
        title: qsTr("Select Demucs ONNX Model")

        onAccepted: {
            Mixxx.StemSeparation.setModelPathFromUrl(selectedFile);
            modelPathField.text = Mixxx.StemSeparation.modelPath;
        }
    }
}
