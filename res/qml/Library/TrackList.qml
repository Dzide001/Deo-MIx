import ".." as Skin
import "." as LibraryComponent
import Mixxx 1.0 as Mixxx
import Qt.labs.qmlmodels
import QtQml
import QtQuick
import QtQml.Models
import QtQuick.Layouts
import QtQuick.Controls 2.15
import "../Theme"

Rectangle {
    id: root

    required property var model

    color: Theme.darkGray

    // M7: lets each row detect whether its track is already loaded on a
    // deck, so the table can highlight it distinctly (acceptance criterion:
    // "a DJ needs to see at a glance which track from the list is already
    // playing"). Iterates however many decks are actually configured
    // rather than hardcoding a deck count.
    Mixxx.ControlProxy {
        id: numDecksControl

        group: "[App]"
        key: "num_decks"
    }
    LibraryComponent.Control {
        id: libraryControl

        onFocusWidgetChanged: {
            switch (focusWidget) {
            case Skin.FocusedWidgetControl.WidgetKind.LibraryView:
                view.forceActiveFocus();
                break;
            }
        }
        onLoadSelectedTrack: (group, play) => {
            view.loadSelectedTrack(group, play);
        }
        onLoadSelectedTrackIntoNextAvailableDeck: play => {
            view.loadSelectedTrackIntoNextAvailableDeck(play);
        }
        onMoveVertical: offset => {
            view.selectionModel.moveSelectionVertical(offset);
        }
    }
    // M7: search filters whichever collection is currently displayed (whole
    // library, a crate, a playlist, a browsed folder) in real time, not a
    // separate global-only search -- it just calls search() on root.model,
    // which is already scoped to the active sidebar selection.
    TextField {
        id: searchField

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 5
        placeholderText: qsTr("Search")

        onTextChanged: {
            if (root.model) {
                root.model.search(text);
            }
        }
        Connections {
            function onModelChanged() {
                searchField.text = root.model ? root.model.currentSearch() : "";
            }

            target: root
        }
    }
    HorizontalHeaderView {
        id: horizontalHeader

        property int sortingColumn: -1
        property var sortingOrder: Qt.Descending

        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: searchField.bottom
        syncView: view

        delegate: Item {
            id: column

            required property string display
            required property int index

            implicitHeight: columnName.contentHeight + 5
            implicitWidth: columnName.contentWidth + 5

            MouseArea {
                id: columnMouseHandler

                acceptedButtons: Qt.LeftButton
                anchors.fill: parent

                onClicked: {
                    if (horizontalHeader.sortingColumn == index) {
                        horizontalHeader.sortingOrder = horizontalHeader.sortingOrder == Qt.DescendingOrder ? Qt.AscendingOrder : Qt.DescendingOrder;
                    } else {
                        horizontalHeader.sortingColumn = index;
                        horizontalHeader.sortingOrder = Qt.AscendingOrder;
                    }
                    view.model.sort(horizontalHeader.sortingColumn, horizontalHeader.sortingOrder);
                }
            }
            Text {
                id: columnName

                anchors.fill: parent
                anchors.leftMargin: 15
                color: Theme.textColor
                elide: Text.ElideRight
                font.capitalization: Font.Capitalize
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignLeft
                text: display
                verticalAlignment: Text.AlignVCenter
            }
            Item {
                anchors {
                    bottom: parent.bottom
                    left: parent.left
                    leftMargin: 5
                    top: parent.top
                }
                Label {
                    id: sortIndicator

                    anchors.centerIn: parent
                    color: "red"
                    elide: Text.ElideRight
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.buttonFontPixelSize
                    horizontalAlignment: Text.AlignRight
                    rotation: horizontalHeader.sortingOrder == Qt.AscendingOrder ? 90 : -90
                    text: "▶"
                    verticalAlignment: Text.AlignVCenter
                    visible: horizontalHeader.sortingColumn == index
                }
            }
            Rectangle {
                id: columnResizer

                color: Theme.darkGray2
                width: 1

                anchors {
                    bottom: parent.bottom
                    right: parent.right
                    top: parent.top
                }
                MouseArea {
                    id: columnResizeHandler

                    property int sizeOffset: 0

                    anchors.fill: parent
                    cursorShape: Qt.SizeHorCursor
                    preventStealing: true

                    onMouseXChanged: {
                        if (drag.active) {
                            column.width += mouseX;
                            sizeOffset += mouseX;
                        }
                    }

                    drag {
                        axis: Drag.XAxis
                        target: parent
                        threshold: 2

                        onActiveChanged: {
                            if (!drag.active && columnResizeHandler.sizeOffset !== 0) {
                                view.model.columns[index].preferredWidth = column.width;
                                columnResizeHandler.sizeOffset = 0;
                                view.updateColumnSize();
                                view.forceLayout();
                            }
                        }
                    }
                }
            }
        }
    }
    TableView {
        id: view

        property int dynamicColumnCount: 0
        property int usedWidth: 0

        function loadSelectedTrack(group, play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return;

            Mixxx.PlayerManager.getPlayer(group).loadTrackFromLocationUrl(urls[0], play);
        }
        function loadSelectedTrackIntoNextAvailableDeck(play) {
            const urls = this.selectionModel.selectedTrackUrls();
            if (urls.length == 0)
                return;

            Mixxx.PlayerManager.loadLocationUrlIntoNextAvailableDeck(urls[0], play);
        }
        function updateColumnSize() {
            const oldUsedWidth = usedWidth;
            const oldDynamicColumnCount = dynamicColumnCount;
            usedWidth = 0;
            dynamicColumnCount = 0;
            if (model == null) {
                return;
            }
            for (let c = 0; c < model.columns.length; c++) {
                if (model.columns[c].hidden || model.columns[c].autoHideWidth > view.width) {
                    continue;
                } else if (model.columns[c].preferredWidth > 0) {
                    usedWidth += model.columns[c].preferredWidth;
                } else {
                    dynamicColumnCount += model.columns[c].fillSpan || 1;
                }
            }
            return oldDynamicColumnCount != dynamicColumnCount || oldUsedWidth != usedWidth;
        }

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 5
        anchors.right: parent.right
        anchors.top: horizontalHeader.bottom
        clip: true
        columnWidthProvider: function (column) {
            const columnDef = view.model.columns[column];
            if (columnDef.hidden) {
                return 0;
            }
            if (columnDef.autoHideWidth > 0 && columnDef.autoHideWidth > view.width) {
                return 0;
            }
            if (columnDef.preferredWidth >= 0) {
                return columnDef.preferredWidth;
            }
            const span = columnDef.fillSpan || 1;
            return span * (view.width - view.usedWidth) / view.dynamicColumnCount;
        }
        keyNavigationEnabled: false
        model: root.model
        pointerNavigationEnabled: false
        reuseItems: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
        }
        delegate: Item {
            id: item

            required property url cover_art
            required property color decoration
            required property var display
            required property string file_url
            required property int row
            required property bool selected
            required property var track

            implicitHeight: 30

            Loader {
                id: loader

                property var capabilities: root.model ? root.model.getCapabilities() : Mixxx.LibraryTrackListModel.Capability.None
                property url cover_art: item.cover_art
                property color decoration: item.decoration
                property var display: item.display
                property url file_url: item.file_url
                property bool isLoaded: {
                    for (let i = 1; i <= numDecksControl.value; i++) {
                        const player = Mixxx.PlayerManager.getPlayer("[Channel" + i + "]");
                        if (player && player.isLoaded && player.currentTrack && player.currentTrack.trackLocationUrl.toString() === item.file_url.toString()) {
                            return true;
                        }
                    }
                    return false;
                }
                property int row: item.row
                property bool selected: item.selected
                property var tableView: view
                property var track: item.track

                anchors.fill: parent
                focus: true
                sourceComponent: delegate

                onLoaded:
                // Workaround needed for WaveformOverview column to load the data
                //     if (track)
                //         Mixxx.Library.analyze(track)
                {}
            }
            // Workaround needed for WaveformOverview column to load the data
            // TableView.onReused: {
            //     if (track)
            //         Mixxx.Library.analyze(track)
            // }
        }
        selectionModel: ItemSelectionModel {
            function moveSelectionVertical(value) {
                if (value == 0)
                    return;

                const selected = this.selectedIndexes;
                const oldRow = (selected.length == 0) ? 0 : selected[0].row;
                this.selectRow(oldRow + value);
            }
            function selectRow(row) {
                const rowCount = this.model.rowCount();
                if (rowCount == 0) {
                    this.clear();
                    return;
                }
                const newRow = Mixxx.MathUtils.positiveModulo(row, rowCount);
                this.select(this.model.index(newRow, 0), ItemSelectionModel.Rows | ItemSelectionModel.Select | ItemSelectionModel.Clear | ItemSelectionModel.Current);
            }
            function selectedTrackUrls() {
                return this.selectedIndexes.map(index => {
                    return this.model.getUrl(index.row);
                });
            }

            model: view.model
        }

        Component.onCompleted: this.updateColumnSize()
        Keys.onDownPressed: this.selectionModel.moveSelectionVertical(1)
        Keys.onEnterPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onReturnPressed: this.loadSelectedTrackIntoNextAvailableDeck(false)
        Keys.onUpPressed: this.selectionModel.moveSelectionVertical(-1)
        onModelChanged: this.updateColumnSize()
        onWidthChanged: {
            if (view.updateColumnSize()) {
                // forceLayout is costly - only invoke if there was a change in the column layouts
                view.forceLayout();
            }
        }
    }
    // Distinct empty state (search-no-results vs. genuinely-empty
    // crate/playlist) rather than just a blank table.
    Label {
        anchors.centerIn: view
        color: Theme.textColor
        font.family: Theme.fontFamily
        font.pixelSize: 14
        text: searchField.text.length > 0 ? qsTr("No tracks match \"%1\"").arg(searchField.text) : qsTr("This collection is empty")
        visible: view.rows === 0
    }
}
