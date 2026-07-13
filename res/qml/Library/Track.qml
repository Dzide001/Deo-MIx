import Mixxx 1.0 as Mixxx
import QtQuick
import QtQuick.Controls 2.15
import QtQuick.Layouts
import "../Theme"

Item {
    id: root

    required property var capabilities
    property alias drag: dragHandler
    readonly property var library: Mixxx.Library
    property alias tap: tapHandler

    function hasCapabilities(caps) {
        return (root.capabilities & caps) == caps;
    }

    DragHandler {
        id: dragHandler

        target: value
    }
    TapHandler {
        id: tapHandler

        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onLongPressed: mouse => {
            contextMenu.popup();
        }
        onTapped: (eventPoint, button) => {
            if (button === Qt.RightButton) {
                contextMenu.popup();
            }
        }
    }
    Menu {
        id: contextMenu

        title: qsTr("File")

        Menu {
            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToDeck) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToSampler) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToPreviewDeck);
            }
            title: qsTr("Load to")

            Menu {
                id: loadToDeckMenu

                enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToDeck)
                title: qsTr("Deck")

                Instantiator {
                    model: 4

                    delegate: MenuItem {
                        text: qsTr("Deck %1").arg(modelData + 1)

                        onTriggered: Mixxx.PlayerManager.getPlayer(`[Channel${modelData + 1}]`).loadTrack(track)
                    }

                    onObjectAdded: (index, object) => loadToDeckMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => loadToDeckMenu.removeItem(object)
                }
            }
            Menu {
                enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.LoadToSampler)
                title: qsTr("Sampler")
            }

            // Instantiator {
            //     id: recentFilesInstantiator
            //     model: settings.recentFiles
            //     delegate: MenuItem {
            //         text: settings.displayableFilePath(modelData)
            //         onTriggered: loadFile(modelData)
            //     }

            //     onObjectAdded: (index, object) => recentFilesMenu.insertItem(index, object)
            //     onObjectRemoved: (index, object) => recentFilesMenu.removeItem(object)
            // }
        }
        Menu {
            id: addToPlaylistMenu

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.AddToTrackSet);
            }
            title: qsTr("Add to playlists")

            Instantiator {
                model: addToPlaylistMenu.visible ? library.playlists() : []

                delegate: MenuItem {
                    text: modelData.name

                    onTriggered: library.addTrackToPlaylist(modelData.id, track)
                }

                onObjectAdded: (index, object) => addToPlaylistMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => addToPlaylistMenu.removeItem(object)
            }
            MenuSeparator {
            }
            MenuItem {
                text: qsTr("Create New Playlist")

                onTriggered: newPlaylistDialog.open()
            }
        }
        Menu {
            id: addToCrateMenu

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.AddToTrackSet);
            }
            title: qsTr("Crates")

            Instantiator {
                model: addToCrateMenu.visible ? library.crates() : []

                delegate: MenuItem {
                    text: modelData.name

                    onTriggered: library.addTrackToCrate(modelData.id, track)
                }

                onObjectAdded: (index, object) => addToCrateMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => addToCrateMenu.removeItem(object)
            }
            MenuSeparator {
            }
            MenuItem {
                text: qsTr("Create New Crate")

                onTriggered: newCrateDialog.open()
            }
        }
        MenuItem {
            enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.EditMetadata)
            text: qsTr("Reload Metadata from File")

            onTriggered: tableView.model.reloadTrackMetadata(row)
        }
        Menu {
            id: assignColorMenu

            title: qsTr("Assign Color")

            Instantiator {
                model: assignColorMenu.visible ? Mixxx.Config.trackColorPalette : []

                delegate: MenuItem {
                    text: modelData.toString()

                    indicator: Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: modelData
                        height: 12
                        width: 12
                        x: 8
                    }

                    onTriggered: track.color = modelData
                }

                onObjectAdded: (index, object) => assignColorMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => assignColorMenu.removeItem(object)
            }
        }
        MenuItem {
            enabled: hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Hide)
            text: qsTr("Hide")

            onTriggered: tableView.model.hideTrack(row)
        }
        Menu {
            id: analyzeMenu

            enabled: {
                hasCapabilities(Mixxx.LibraryTrackListModel.Capability.EditMetadata) || hasCapabilities(Mixxx.LibraryTrackListModel.Capability.Analyze);
            }
            title: qsTr("Analyze")

            MenuItem {
                text: qsTr("Analyze")

                onTriggered: {
                    library.analyze(track);
                }
            }
            MenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze")
            }
            MenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze (constant BPM)")
            }
            MenuItem {
                enabled: false // TODO implement
                text: qsTr("Reanalyze (variable BPM)")
            }
        }
    }
    // Minimal "type a name, create it, and add this track to it" dialogs
    // for the context menu's "Create New Playlist/Crate" actions.
    Popup {
        id: newPlaylistDialog

        focus: true
        modal: true
        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        ColumnLayout {
            TextField {
                id: newPlaylistName

                Layout.preferredWidth: 220
                placeholderText: qsTr("Playlist name")

                onAccepted: createPlaylistButton.clicked()
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight

                Button {
                    text: qsTr("Cancel")

                    onClicked: newPlaylistDialog.close()
                }
                Button {
                    id: createPlaylistButton

                    enabled: newPlaylistName.text.length > 0
                    text: qsTr("Create")

                    onClicked: {
                        const id = library.createPlaylist(newPlaylistName.text);
                        if (id >= 0) {
                            library.addTrackToPlaylist(id, track);
                        }
                        newPlaylistName.text = "";
                        newPlaylistDialog.close();
                    }
                }
            }
        }
        onOpened: newPlaylistName.forceActiveFocus()
    }
    Popup {
        id: newCrateDialog

        focus: true
        modal: true
        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        ColumnLayout {
            TextField {
                id: newCrateName

                Layout.preferredWidth: 220
                placeholderText: qsTr("Crate name")

                onAccepted: createCrateButton.clicked()
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight

                Button {
                    text: qsTr("Cancel")

                    onClicked: newCrateDialog.close()
                }
                Button {
                    id: createCrateButton

                    enabled: newCrateName.text.length > 0
                    text: qsTr("Create")

                    onClicked: {
                        const id = library.createCrate(newCrateName.text);
                        if (id >= 0) {
                            library.addTrackToCrate(id, track);
                        }
                        newCrateName.text = "";
                        newCrateDialog.close();
                    }
                }
            }
        }
        onOpened: newCrateName.forceActiveFocus()
    }
}
