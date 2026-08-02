import QtQuick 2.12
import QtQuick.Window
import QtQuick.Controls 2.12
import Mixxx 1.0 as Mixxx

// M14 Stage 6: the singer-facing second-screen output -- modeled directly
// on VideoPreviewPanel.qml's proven pattern (a plain top-level Window
// declared as a child item; Qt Quick treats any Window-derived item
// anywhere in the tree as its own independent OS window, confirmed
// working for M12's video preview including moving it to another
// monitor). Unlike that DJ-facing preview, this is meant to fill an
// entire dedicated singer-facing display, not float as a small window.
//
// Shows whichever deck is the "active performer": whichever deck is
// currently playing; if both are, whichever most recently started
// (simplest reasonable rule for a 2-deck app where karaoke is normally
// one performer at a time, with the other deck available for the next
// transition).
Window {
    id: root

    // Index into Qt.application.screens, chosen via TopBar.qml's screen
    // picker -- stored on Mixxx.KaraokeManager since it's shared UI state
    // between that picker and this window, two separate component trees.
    //
    // Diagnostic finding: binding this Window's own `screen` property
    // directly to a Qt.application.screens[] entry made the whole window
    // silently never appear at all (no QML warning logged) -- reassigning
    // a Window's `screen` property live is apparently unsupported/unsafe
    // in this Qt build. Fixed by never touching `screen` at all and
    // instead just positioning/sizing the window to cover the target
    // screen's geometry directly -- Qt derives the effective screen from
    // where the window actually ends up on the desktop, which is all a
    // singer-facing full-screen display actually needs.
    readonly property int screenIndex: Mixxx.KaraokeManager.karaokeDisplayScreenIndex
    readonly property var targetScreen: (screenIndex >= 0 && screenIndex < Qt.application.screens.length) ? Qt.application.screens[screenIndex] : Qt.application.screens[0]

    color: "black"
    height: targetScreen ? targetScreen.height : 600
    title: qsTr("Karaoke Display")
    visible: Mixxx.KaraokeManager.karaokeModeEnabled
    width: targetScreen ? targetScreen.width : 800
    x: targetScreen ? targetScreen.virtualX : 100
    y: targetScreen ? targetScreen.virtualY : 100

    Mixxx.ControlProxy {
        id: deckAPlay

        group: "[Channel1]"
        key: "play"
    }
    Mixxx.ControlProxy {
        id: deckBPlay

        group: "[Channel2]"
        key: "play"
    }
    Mixxx.ControlProxy {
        id: deckADuration

        group: "[Channel1]"
        key: "duration"
    }
    Mixxx.ControlProxy {
        id: deckAPosition

        group: "[Channel1]"
        key: "playposition"
    }
    Mixxx.ControlProxy {
        id: deckBDuration

        group: "[Channel2]"
        key: "duration"
    }
    Mixxx.ControlProxy {
        id: deckBPosition

        group: "[Channel2]"
        key: "playposition"
    }
    // M16 fix: transcription should be offerable for a loaded-but-not-yet-
    // playing track too (prep lyrics ahead of a set, not only mid-song) --
    // track_loaded is a real EngineBuffer CO (src/engine/enginebuffer.cpp),
    // 1 whenever a deck has a track loaded regardless of play state.
    Mixxx.ControlProxy {
        id: deckALoaded

        group: "[Channel1]"
        key: "track_loaded"
    }
    Mixxx.ControlProxy {
        id: deckBLoaded

        group: "[Channel2]"
        key: "track_loaded"
    }

    property double deckALastStartedAt: 0
    property double deckBLastStartedAt: 0

    // play has a real NOTIFY signal (valueChanged), so tracking "which
    // deck most recently started" this way is safe and live -- a
    // different situation from the Q_INVOKABLE-with-no-NOTIFY methods
    // (hasLyrics/currentLine/etc.) this whole feature otherwise has to
    // poll from a Timer instead of binding to directly.
    Connections {
        function onValueChanged(value) {
            if (value > 0) {
                root.deckALastStartedAt = Date.now();
            }
        }

        target: deckAPlay
    }
    Connections {
        function onValueChanged(value) {
            if (value > 0) {
                root.deckBLastStartedAt = Date.now();
            }
        }

        target: deckBPlay
    }

    readonly property bool deckAPlaying: deckAPlay.value > 0
    readonly property bool deckBPlaying: deckBPlay.value > 0
    readonly property bool deckALoadedTrack: deckALoaded.value > 0
    readonly property bool deckBLoadedTrack: deckBLoaded.value > 0
    readonly property string activeDeckGroup: {
        if (root.deckAPlaying && !root.deckBPlaying)
            return "[Channel1]";
        if (root.deckBPlaying && !root.deckAPlaying)
            return "[Channel2]";
        if (root.deckAPlaying && root.deckBPlaying)
            return root.deckALastStartedAt >= root.deckBLastStartedAt ? "[Channel1]" : "[Channel2]";
        // Neither deck is playing -- fall back to whichever has a track
        // loaded, so lyrics can be checked/transcribed ahead of a set
        // rather than only once a deck actually starts playing.
        if (root.deckALoadedTrack && !root.deckBLoadedTrack)
            return "[Channel1]";
        if (root.deckBLoadedTrack && !root.deckALoadedTrack)
            return "[Channel2]";
        if (root.deckALoadedTrack && root.deckBLoadedTrack)
            return root.deckALastStartedAt >= root.deckBLastStartedAt ? "[Channel1]" : "[Channel2]";
        return "";
    }
    readonly property double activePositionSeconds: {
        if (root.activeDeckGroup === "[Channel1]")
            return deckADuration.value * deckAPosition.value;
        if (root.activeDeckGroup === "[Channel2]")
            return deckBDuration.value * deckBPosition.value;
        return 0;
    }

    property bool hasLyrics: false
    property bool hasCdg: false
    property string lyricLine: ""
    property string nextUpText: ""

    // M16 Stage 3: Mixxx.LyricTranscription is always a registered QML
    // singleton (see qmllyrictranscriptionproxy.cpp); when the app wasn't
    // built with AI_LYRIC_TRANSCRIPTION it just stays permanently idle
    // (isRunning always false, transcribeLyrics() a no-op) rather than
    // being undefined, matching Mixxx.StemSeparation's own precedent
    // (StemsBankContent.qml).
    property string transcriptionError: ""

    Connections {
        target: Mixxx.LyricTranscription

        function onFailed(message) {
            root.transcriptionError = message;
            transcriptionErrorTimer.restart();
        }
    }
    Timer {
        id: transcriptionErrorTimer

        interval: 4000
        onTriggered: root.transcriptionError = ""
    }

    function styledLine(line, wordIndex) {
        if (wordIndex < 0 || line === "") {
            return line;
        }
        const words = line.split(' ');
        const sung = words.slice(0, wordIndex + 1).join(' ');
        const upcoming = words.slice(wordIndex + 1).join(' ');
        let html = '<font color="#FFD700">' + sung + '</font>';
        if (upcoming.length > 0) {
            html += ' <font color="#FFFFFF">' + upcoming + '</font>';
        }
        return html;
    }

    // M15b: gated on the window actually being shown. This window is
    // instantiated unconditionally from main.qml and only made visible
    // while karaoke mode is on, but a Timer keeps firing regardless of
    // its window's visibility -- so this was polling KaraokeManager
    // every 150ms for the entire life of every session, to update a
    // window nobody could see. Nothing here is needed while hidden: the
    // handler only writes properties that this window's own (also
    // hidden) items render, and it re-runs immediately on show thanks to
    // triggeredOnStart.
    Timer {
        interval: 150
        repeat: true
        running: root.visible
        triggeredOnStart: true

        onTriggered: {
            if (root.activeDeckGroup === "") {
                root.hasLyrics = false;
                root.hasCdg = false;
                root.lyricLine = "";
            } else {
                root.hasLyrics = Mixxx.KaraokeManager.hasLyrics(root.activeDeckGroup);
                root.hasCdg = Mixxx.KaraokeManager.hasCdgSource(root.activeDeckGroup);
                if (root.hasLyrics && !root.hasCdg) {
                    const rawLine = Mixxx.KaraokeManager.currentLine(root.activeDeckGroup, root.activePositionSeconds);
                    const wordIndex = Mixxx.KaraokeManager.currentWordIndex(root.activeDeckGroup, root.activePositionSeconds);
                    root.lyricLine = root.styledLine(rawLine, wordIndex);
                } else {
                    root.lyricLine = "";
                }
            }

            const queue = Mixxx.KaraokeManager.singerQueue;
            const nextRow = queue.nextWaitingRow();
            if (nextRow < 0) {
                root.nextUpText = "";
            } else {
                const entry = queue.get(nextRow);
                root.nextUpText = qsTr("Up next: ") + entry.singerName + (entry.songRequest.length > 0 ? (" — " + entry.songRequest) : "");
            }
        }
    }

    // CDG preview, letterboxed to preserve its fixed 300x216 aspect ratio.
    Item {
        readonly property real aspect: 300 / 216

        anchors.centerIn: parent
        height: (parent.width / parent.height > aspect) ? parent.height : parent.width / aspect
        visible: root.hasCdg
        width: (parent.width / parent.height > aspect) ? parent.height * aspect : parent.width

        Mixxx.CdgPreview {
            anchors.fill: parent
            group: root.activeDeckGroup
            positionSeconds: root.activePositionSeconds
        }
    }

    // Large, centered text lyric display with word-level highlight
    // (styledLine() above) when the active source is enhanced LRC.
    Text {
        anchors.centerIn: parent
        color: "white"
        font.bold: true
        font.pixelSize: Math.max(24, root.height * 0.08)
        horizontalAlignment: Text.AlignHCenter
        text: root.lyricLine
        textFormat: Text.StyledText
        visible: root.hasLyrics && !root.hasCdg
        width: parent.width * 0.85
        wrapMode: Text.WordWrap
    }

    // Spec's "clear no lyrics available state, not blank/broken" --
    // this is the one place in the whole feature that needs an explicit
    // empty state (see LyricDisplay.qml's own comment for why the
    // smaller DJ-facing strip just collapses away instead).
    Column {
        anchors.centerIn: parent
        spacing: 16
        visible: !root.hasLyrics && !root.hasCdg

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#808080"
            font.pixelSize: 28
            text: root.activeDeckGroup === "" ? qsTr("No track loaded") : qsTr("No lyrics available")
        }

        // M16 Stage 3/lrclib: on-demand lyric acquisition, offered right
        // where the "no lyrics" empty state already is -- the natural,
        // discoverable place for it (only makes sense once a track is
        // actually loaded, hence gating on activeDeckGroup rather than
        // always showing). Tries a free lrclib.net lookup first, falling
        // back to local AI transcription only if that finds no match
        // (WhisperTranscriptionManager handles the two-stage logic) --
        // labeled "Get Lyrics" rather than "Transcribe Lyrics" since it
        // may not need to transcribe anything at all.
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            enabled: root.activeDeckGroup !== "" && !Mixxx.LyricTranscription.isRunning
            text: {
                if (Mixxx.LyricTranscription.isRunning) {
                    return Mixxx.LyricTranscription.statusMessage || qsTr("Working...");
                }
                return root.transcriptionError ? qsTr("Failed - Retry") : qsTr("Get Lyrics");
            }
            visible: root.activeDeckGroup !== ""

            onClicked: {
                root.transcriptionError = "";
                const player = Mixxx.PlayerManager.getPlayer(root.activeDeckGroup);
                if (player && player.currentTrack) {
                    Mixxx.LyricTranscription.transcribeLyrics(player.currentTrack, root.activeDeckGroup);
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#B4453F"
            font.pixelSize: 16
            text: root.transcriptionError
            visible: root.transcriptionError.length > 0
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#AA000000"
        height: 50
        visible: root.nextUpText.length > 0

        Label {
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 18
            text: root.nextUpText
        }
    }
}
