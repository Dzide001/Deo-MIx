import QtQuick 2.12
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import "../Theme"

// M14 Stage 1: compact in-app lyric strip, showing each deck's current
// synced lyric line from a .lrc sidecar file next to its loaded track
// (same base filename, same folder -- KaraokeManager, src/library/
// karaoke/, handles the sidecar lookup/parsing and is queried here purely
// by group + playback position).
//
// Collapses to zero height when neither deck currently has a usable
// lyrics source, so ordinary (non-karaoke) use is completely unaffected --
// this is why it participates directly in rootColumn's layout (main.qml)
// rather than needing Stage 4's karaoke-mode toggle to gate it at all;
// that toggle instead controls auto-mute and the second-screen output,
// not whether this strip can ever show something. The spec's "clear no
// lyrics available state, not blank/broken" requirement is about the
// dedicated singer-facing KaraokeDisplayWindow (Stage 6) rather than this
// small DJ-facing monitoring strip -- collapsing away when there's
// nothing to show is the right behavior for an ambient status strip like
// this one, matching VideoPreviewPanel.qml's own
// `visible: Mixxx.VideoEngine.enabled` precedent.
Item {
    id: root

    // Plain properties, updated imperatively from the poll Timer below --
    // NOT declarative bindings straight to Mixxx.KaraokeManager.hasLyrics().
    // hasLyrics()/currentLine() are Q_INVOKABLE calls with no NOTIFY
    // signal, so a binding like `deckAHasLyrics: Mixxx.KaraokeManager.
    // hasLyrics("[Channel1]")` only ever evaluates once at this Item's
    // creation (before any track is loaded) and then never again, no
    // matter how many tracks load afterward -- confirmed via a real
    // manual test: KaraokeManager's own C++ side correctly detected and
    // parsed the sidecar file every time, but this strip stayed invisible
    // because deckAHasLyrics had already latched onto its startup value.
    property bool deckAHasLyrics: false
    property bool deckBHasLyrics: false
    // M14 Stage 3: hasLyrics() is true for either an LRC or a CDG source
    // (both just mean "isValid()"), so these separately track which kind
    // is active per deck -- CDG needs the image preview instead of the
    // Text item below, and wants more vertical room to be legible.
    property bool deckAHasCdg: false
    property bool deckBHasCdg: false
    readonly property bool anyLyrics: root.deckAHasLyrics || root.deckBHasLyrics
    readonly property bool anyCdg: root.deckAHasCdg || root.deckBHasCdg

    property string deckALine: ""
    property string deckBLine: ""

    // M14 Stage 2: word-level (enhanced LRC) highlight, built as a rich-
    // text string with the already-"sung" words in the deck's accent
    // color and the rest dimmed -- a plain karaoke bouncing-ball look.
    // Built imperatively in JS (styledLine() below) rather than a
    // Repeater over words: this file already has one confirmed instance
    // of a QML binding-to-invokable-with-no-NOTIFY bug (see the comment
    // above deckAHasLyrics), and per M4's own hard-won lesson, Repeaters
    // over many small delegates have been a real source of intermittent
    // Qt Quick bugs in this codebase -- a single Text with StyledText
    // sidesteps both classes of risk for something this simple.
    function styledLine(line, wordIndex, sungColor, upcomingColor) {
        if (wordIndex < 0 || line === "") {
            return line;
        }
        const words = line.split(' ');
        const sung = words.slice(0, wordIndex + 1).join(' ');
        const upcoming = words.slice(wordIndex + 1).join(' ');
        let html = '<font color="' + sungColor + '">' + sung + '</font>';
        if (upcoming.length > 0) {
            html += ' <font color="' + upcomingColor + '">' + upcoming + '</font>';
        }
        return html;
    }

    readonly property int stripHeight: root.anyCdg ? 90 : (root.anyLyrics ? 36 : 0)

    Layout.fillWidth: true
    Layout.maximumHeight: root.stripHeight
    Layout.preferredHeight: root.stripHeight
    clip: true

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

    // Real Qt properties with NOTIFY (unlike hasLyrics()/currentLine()),
    // so these two are safe to bind reactively -- QmlCdgPreviewItem's own
    // positionSeconds below stays live without needing the poll Timer.
    readonly property double deckAPositionSeconds: deckADuration.value * deckAPosition.value
    readonly property double deckBPositionSeconds: deckBDuration.value * deckBPosition.value

    // Lyric lines change on the order of seconds, nowhere near audio/video
    // frame rates -- 150ms is plenty responsive without adding meaningful
    // CPU cost. Poll-from-QML rather than a push notification from
    // KaraokeManager, matching the same pattern M12's preview items and
    // M6's waveform overview already use for state that isn't a plain Qt
    // property.
    //
    // M15b: the interval is now adaptive. This timer is always running
    // (it's what DETECTS that lyrics appeared, so it can't be gated on
    // lyrics existing without becoming circular), but at a flat 150ms it
    // cost ~27 QML->C++ Q_INVOKABLE calls/second forever -- including on
    // every ordinary non-karaoke session, where all four calls just
    // return false every time. Only the line/word highlighting actually
    // needs to be fast; simply noticing that a track with lyrics got
    // loaded does not. So: 150ms while either deck has lyrics, 1s
    // otherwise, which cuts the idle cost by ~85% with no perceptible
    // difference (a newly loaded lyric track shows up within a second,
    // still faster than a human notices, and highlighting is unchanged
    // once it does).
    Timer {
        interval: (root.deckAHasLyrics || root.deckBHasLyrics) ? 150 : 1000
        repeat: true
        running: true
        triggeredOnStart: true

        onTriggered: {
            root.deckAHasLyrics = Mixxx.KaraokeManager.hasLyrics("[Channel1]");
            root.deckBHasLyrics = Mixxx.KaraokeManager.hasLyrics("[Channel2]");
            root.deckAHasCdg = Mixxx.KaraokeManager.hasCdgSource("[Channel1]");
            root.deckBHasCdg = Mixxx.KaraokeManager.hasCdgSource("[Channel2]");

            if (root.deckAHasLyrics && !root.deckAHasCdg) {
                const deckARawLine = Mixxx.KaraokeManager.currentLine("[Channel1]", root.deckAPositionSeconds);
                const deckAWordIndex = Mixxx.KaraokeManager.currentWordIndex("[Channel1]", root.deckAPositionSeconds);
                root.deckALine = root.styledLine(deckARawLine, deckAWordIndex, Theme.deckAAccent, Theme.deckAAccentDim);
            } else {
                root.deckALine = "";
            }

            if (root.deckBHasLyrics && !root.deckBHasCdg) {
                const deckBRawLine = Mixxx.KaraokeManager.currentLine("[Channel2]", root.deckBPositionSeconds);
                const deckBWordIndex = Mixxx.KaraokeManager.currentWordIndex("[Channel2]", root.deckBPositionSeconds);
                root.deckBLine = root.styledLine(deckBRawLine, deckBWordIndex, Theme.deckBAccent, Theme.deckBAccentDim);
            } else {
                root.deckBLine = "";
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 8

        // M14 Stage 3: a CDG source has no text at all -- currentLine()
        // always returns "" for it (see CdgLyricsSource) -- so its deck
        // shows the decoded image preview instead of the Text item below.
        // 4:3-ish thumbnail (CDG's native canvas is 300x216), modest but
        // legible; the real viewing surface for CDG content is Stage 6's
        // dedicated, full-size KaraokeDisplayWindow, not this compact
        // DJ-facing monitoring strip.
        Mixxx.CdgPreview {
            // CD+G's canvas is a fixed 300x216 (CdgDecoder::kWidth/
            // kHeight) -- hardcoding that ratio here rather than querying
            // it, since it never varies per-source the way M12's video
            // preview's delivery resolution conceptually could.
            Layout.preferredHeight: root.stripHeight
            Layout.preferredWidth: root.stripHeight * (300 / 216)
            group: "[Channel1]"
            positionSeconds: root.deckAPositionSeconds
            visible: root.deckAHasCdg
        }
        Mixxx.CdgPreview {
            Layout.preferredHeight: root.stripHeight
            Layout.preferredWidth: root.stripHeight * (300 / 216)
            group: "[Channel2]"
            positionSeconds: root.deckBPositionSeconds
            visible: root.deckBHasCdg
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                color: Theme.deckAAccent
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textFontPixelSize
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.StyledText
                text: root.deckALine
                visible: root.deckAHasLyrics && !root.deckAHasCdg
            }
            Text {
                Layout.fillWidth: true
                color: Theme.deckBAccent
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.textFontPixelSize
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.StyledText
                text: root.deckBLine
                visible: root.deckBHasLyrics && !root.deckBHasCdg
            }
        }
    }
}
