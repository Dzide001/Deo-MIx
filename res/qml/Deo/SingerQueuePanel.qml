import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../Theme"

// M14 Stage 5: singer queue -- add/reorder/mark-performing/mark-done/
// mark-skipped, bound to the real Mixxx.KaraokeManager.singerQueue model
// (QmlSingerQueueModel, src/qml/qmlsingerqueuemodel.h/.cpp). A plain
// ListView over a real QAbstractListModel, not a Repeater over a JS
// array -- a different, standard mechanism from the Repeater/
// required-property instability M4's pad banks hit (that was tied to
// many Skin.ControlButton-derived delegates with several required
// properties each, all instantiated simultaneously at app startup; this
// list is a handful of plain Rectangle/Label delegates, created on demand
// as entries are added, one at a time).
//
// A plain embedded Rectangle (not a Popup) -- per explicit user request,
// this lives as a permanent sidebar next to the library/browser section
// (main.qml) rather than a modal dialog, so it's visible the whole time
// karaoke mode is on instead of needing to be reopened.
Rectangle {
    id: root

    readonly property var queue: Mixxx.KaraokeManager.singerQueue

    border.color: Theme.darkGray3
    border.width: 1
    color: Theme.deckPanelBackground
    radius: 6

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            color: Theme.deckTextBright
            font.bold: true
            font.pixelSize: 16
            text: qsTr("Singer Queue")
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            TextField {
                id: singerNameField

                Layout.preferredWidth: 130
                placeholderText: qsTr("Singer name")
            }
            TextField {
                id: songRequestField

                Layout.fillWidth: true
                placeholderText: qsTr("Song request")

                onAccepted: addButton.clicked()
            }
            Button {
                id: addButton

                enabled: singerNameField.text.length > 0
                text: qsTr("Add")

                onClicked: {
                    root.queue.addEntry(singerNameField.text, songRequestField.text);
                    singerNameField.text = "";
                    songRequestField.text = "";
                    singerNameField.forceActiveFocus();
                }
            }
        }

        ListView {
            id: queueList

            Layout.fillHeight: true
            Layout.fillWidth: true
            clip: true
            model: root.queue
            spacing: 4

            delegate: Rectangle {
                // Status: 0=Waiting, 1=Performing, 2=Done, 3=Skipped --
                // see QmlSingerQueueModel::Status. "up next" isn't a
                // stored status -- it's whichever Waiting row
                // nextWaitingRow() currently reports, computed live here.
                readonly property bool isNextUp: status === 0 && index === root.queue.nextWaitingRow()
                readonly property bool isFinished: status === 2 || status === 3

                color: {
                    if (status === 1) {
                        return Theme.deckActiveColor;
                    }
                    if (isFinished) {
                        return Theme.darkGray2;
                    }
                    if (isNextUp) {
                        return Theme.accentColor;
                    }
                    return Theme.deckPanelAltBackground;
                }
                height: 56
                opacity: isFinished ? 0.5 : 1.0
                radius: 4
                width: queueList.width

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 6

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Label {
                            Layout.fillWidth: true
                            color: Theme.deckTextBright
                            elide: Text.ElideRight
                            font.bold: true
                            text: singerName
                        }
                        Label {
                            Layout.fillWidth: true
                            color: Theme.deckTextSecondary
                            elide: Text.ElideRight
                            font.pixelSize: 11
                            text: songRequest
                        }
                    }
                    Label {
                        color: Theme.deckTextSecondary
                        font.pixelSize: 10
                        text: {
                            if (status === 1)
                                return qsTr("PERFORMING");
                            if (status === 2)
                                return qsTr("DONE");
                            if (status === 3)
                                return qsTr("SKIPPED");
                            return isNextUp ? qsTr("UP NEXT") : qsTr("WAITING");
                        }
                    }
                    Button {
                        enabled: index > 0
                        implicitWidth: 26
                        text: "▲"

                        onClicked: root.queue.moveEntry(index, index - 1)
                    }
                    Button {
                        enabled: index < queueList.count - 1
                        implicitWidth: 26
                        text: "▼"

                        onClicked: root.queue.moveEntry(index, index + 1)
                    }
                    Button {
                        implicitWidth: 26
                        text: "♪"

                        onClicked: root.queue.markPerforming(index)
                    }
                    Button {
                        implicitWidth: 26
                        text: "✓"

                        onClicked: root.queue.markDone(index)
                    }
                    Button {
                        implicitWidth: 26
                        text: "⤫"

                        onClicked: root.queue.markSkipped(index)
                    }
                    Button {
                        implicitWidth: 26
                        text: "✕"

                        onClicked: root.queue.removeEntry(index)
                    }
                }
            }
        }
    }
}
