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

        // M7/column-visibility: right-click any visible column's header to
        // show/hide any of the real columns (including currently-hidden
        // ones -- necessary since a hidden column has no header cell of
        // its own to right-click on). One shared menu, not per-column,
        // matching CustomActionPicker.qml's own "one shared popup, opened
        // by whichever thing was clicked" precedent from the pad-bank
        // feature. Hand-written entries rather than a Repeater over
        // `view.model.columns` -- looked up by columnIdx (Mixxx.TrackListColumn.SQLColumns.*)
        // rather than assuming array position, so this stays correct even
        // if SourceTree.qml's column order changes later.
        //
        // checked/text are set imperatively (onAboutToShow, and again
        // inside each item's own onTriggered) rather than via a live
        // `checked: !column.hidden` binding -- `hidden` is a plain MEMBER
        // property with no NOTIFY signal (same as the existing
        // preferredWidth resize path below), and Qt Quick Controls'
        // checkable MenuItem overwrites `checked` as a concrete value the
        // first time it's clicked, which would silently break a live
        // binding on the very first toggle.
        Menu {
            id: columnVisibilityMenu

            function findColumn(sqlColumnEnum) {
                if (!view.model) {
                    return null;
                }
                for (let i = 0; i < view.model.columns.length; i++) {
                    if (view.model.columns[i].columnIdx === sqlColumnEnum) {
                        return view.model.columns[i];
                    }
                }
                return null;
            }
            // Refuses to hide the LAST currently-visible column -- with
            // none left, there would be no header cell left to right-click
            // to ever bring this menu back.
            function toggleColumn(menuItem, sqlColumnEnum) {
                const col = columnVisibilityMenu.findColumn(sqlColumnEnum);
                if (!col) {
                    return;
                }
                if (!col.hidden) {
                    let visibleCount = 0;
                    for (let i = 0; i < view.model.columns.length; i++) {
                        if (!view.model.columns[i].hidden) {
                            visibleCount++;
                        }
                    }
                    if (visibleCount <= 1) {
                        menuItem.checked = true;
                        return;
                    }
                }
                col.hidden = !col.hidden;
                menuItem.checked = !col.hidden;
                Mixxx.LibraryColumnSettings.setColumnHidden(col.columnIdx, col.hidden);
                view.updateColumnSize();
                view.forceLayout();
            }

            onAboutToShow: {
                titleMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Title).hidden;
                artistMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Artist).hidden;
                albumMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Album).hidden;
                yearMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Year).hidden;
                bpmMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Bpm).hidden;
                keyMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Key).hidden;
                fileTypeMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.FileType).hidden;
                bitrateMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Bitrate).hidden;
                genreMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Genre).hidden;
                composerMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Composer).hidden;
                commentMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Comment).hidden;
                durationMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Duration).hidden;
                dateAddedMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.DateAdded).hidden;
                timesPlayedMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.TimesPlayed).hidden;
                ratingMenuItem.checked = !columnVisibilityMenu.findColumn(Mixxx.TrackListColumn.SQLColumns.Rating).hidden;
            }

            MenuItem {
                id: titleMenuItem

                checkable: true
                text: qsTr("Title")

                onTriggered: columnVisibilityMenu.toggleColumn(titleMenuItem, Mixxx.TrackListColumn.SQLColumns.Title)
            }
            MenuItem {
                id: artistMenuItem

                checkable: true
                text: qsTr("Artist")

                onTriggered: columnVisibilityMenu.toggleColumn(artistMenuItem, Mixxx.TrackListColumn.SQLColumns.Artist)
            }
            MenuItem {
                id: albumMenuItem

                checkable: true
                text: qsTr("Album")

                onTriggered: columnVisibilityMenu.toggleColumn(albumMenuItem, Mixxx.TrackListColumn.SQLColumns.Album)
            }
            MenuItem {
                id: yearMenuItem

                checkable: true
                text: qsTr("Year")

                onTriggered: columnVisibilityMenu.toggleColumn(yearMenuItem, Mixxx.TrackListColumn.SQLColumns.Year)
            }
            MenuItem {
                id: bpmMenuItem

                checkable: true
                text: qsTr("Bpm")

                onTriggered: columnVisibilityMenu.toggleColumn(bpmMenuItem, Mixxx.TrackListColumn.SQLColumns.Bpm)
            }
            MenuItem {
                id: keyMenuItem

                checkable: true
                text: qsTr("Key")

                onTriggered: columnVisibilityMenu.toggleColumn(keyMenuItem, Mixxx.TrackListColumn.SQLColumns.Key)
            }
            MenuItem {
                id: fileTypeMenuItem

                checkable: true
                text: qsTr("File Type")

                onTriggered: columnVisibilityMenu.toggleColumn(fileTypeMenuItem, Mixxx.TrackListColumn.SQLColumns.FileType)
            }
            MenuItem {
                id: bitrateMenuItem

                checkable: true
                text: qsTr("Bitrate")

                onTriggered: columnVisibilityMenu.toggleColumn(bitrateMenuItem, Mixxx.TrackListColumn.SQLColumns.Bitrate)
            }
            MenuItem {
                id: genreMenuItem

                checkable: true
                text: qsTr("Genre")

                onTriggered: columnVisibilityMenu.toggleColumn(genreMenuItem, Mixxx.TrackListColumn.SQLColumns.Genre)
            }
            MenuItem {
                id: composerMenuItem

                checkable: true
                text: qsTr("Composer")

                onTriggered: columnVisibilityMenu.toggleColumn(composerMenuItem, Mixxx.TrackListColumn.SQLColumns.Composer)
            }
            MenuItem {
                id: commentMenuItem

                checkable: true
                text: qsTr("Comment")

                onTriggered: columnVisibilityMenu.toggleColumn(commentMenuItem, Mixxx.TrackListColumn.SQLColumns.Comment)
            }
            MenuItem {
                id: durationMenuItem

                checkable: true
                text: qsTr("Duration")

                onTriggered: columnVisibilityMenu.toggleColumn(durationMenuItem, Mixxx.TrackListColumn.SQLColumns.Duration)
            }
            MenuItem {
                id: dateAddedMenuItem

                checkable: true
                text: qsTr("Date Added")

                onTriggered: columnVisibilityMenu.toggleColumn(dateAddedMenuItem, Mixxx.TrackListColumn.SQLColumns.DateAdded)
            }
            MenuItem {
                id: timesPlayedMenuItem

                checkable: true
                text: qsTr("Times Played")

                onTriggered: columnVisibilityMenu.toggleColumn(timesPlayedMenuItem, Mixxx.TrackListColumn.SQLColumns.TimesPlayed)
            }
            MenuItem {
                id: ratingMenuItem

                checkable: true
                text: qsTr("Rating")

                onTriggered: columnVisibilityMenu.toggleColumn(ratingMenuItem, Mixxx.TrackListColumn.SQLColumns.Rating)
            }
        }

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
            MouseArea {
                id: columnVisibilityHandler

                acceptedButtons: Qt.RightButton
                anchors.fill: parent

                onClicked: columnVisibilityMenu.popup()
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
