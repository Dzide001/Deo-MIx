#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>
#include <memory>

#include "track/track_decl.h"

namespace mixxx {

class LyricsSource;

/// M14: owns the per-deck lyrics lifecycle -- on every track load (any
/// deck, however it got loaded: drag, double-click, AutoDJ, ...), looks
/// for a sidecar lyric file next to the track (same base filename, same
/// folder -- the subtitle-file convention), tries `.lrc` first, then
/// (Stage 3) `.cdg`, and holds whichever LyricsSource resulted so the
/// QML-facing proxy can query "what line is showing right now" without
/// re-parsing anything.
///
/// Modeled on VideoEngineManager (src/library/videoengine/) for the
/// overall shape (owned by CoreServices, exposed to QML via a thin
/// seeded-shared_ptr proxy -- see QmlKaraokeManagerProxy), but does not
/// need any of that class's ControlObject/GStreamer machinery: track-load
/// detection here hooks PlayerInfo::instance()'s real, existing
/// `trackChanged` signal instead, since a sidecar file path isn't
/// something a numeric ControlObject could carry anyway.
class KaraokeManager : public QObject {
    Q_OBJECT
  public:
    explicit KaraokeManager(QObject* pParent);
    // Defined in the .cpp (not = default here): m_sourceByDeck holds
    // std::shared_ptr<LyricsSource>, and LyricsSource is only
    // forward-declared in this header, so its destructor must be
    // instantiated somewhere LyricsSource is a complete type.
    ~KaraokeManager() override;

    /// True if `deckGroup` (e.g. "[Channel1]") currently has a usable
    /// lyrics source loaded.
    bool hasLyrics(const QString& deckGroup) const;

    /// The line that should be showing on this deck at this playback
    /// position, or an empty string if there's no lyrics source or no
    /// line is active yet.
    QString currentLine(const QString& deckGroup, double positionSeconds) const;

    /// M14 Stage 2: the index (into currentLine()'s text, split on single
    /// spaces) of the word that should currently be highlighted, or -1 if
    /// this deck's lyrics source isn't an enhanced/word-level LRC file (or
    /// this particular line has no word-level data, or no word is active
    /// yet). LyricsSource itself has no notion of "words" -- that's
    /// LRC-specific, so this checks whether the concrete source happens to
    /// be an LrcLyricsSource rather than exposing it on the shared
    /// interface (a future CdgLyricsSource has nothing analogous to
    /// offer).
    int currentWordIndex(const QString& deckGroup, double positionSeconds) const;

    /// M14 Stage 3: true if `deckGroup`'s current lyrics source is a
    /// decoded .cdg file (as opposed to an .lrc one, or none) -- QML uses
    /// this to decide whether to show the text-based LyricDisplay strip
    /// or the image-based CDG preview for this deck.
    bool hasCdgSource(const QString& deckGroup) const;

    /// The current CD+G canvas frame for this deck at this playback
    /// position, or a null QImage if this deck's source isn't a .cdg file
    /// or has no frame data.
    QImage currentCdgFrame(const QString& deckGroup, double positionSeconds) const;

    /// M14 Stage 4: whether karaoke mode is currently on. Purely a
    /// session-scoped flag on this object (not persisted, not a
    /// ControlObject) -- gates auto-mute-on-load below and, in QML,
    /// whether the second-screen KaraokeDisplayWindow/SingerQueuePanel
    /// are shown. Does not retroactively affect a track already loaded
    /// when it's toggled on.
    bool isKaraokeModeEnabled() const {
        return m_karaokeModeEnabled;
    }
    void setKaraokeModeEnabled(bool enabled);

    /// M16: re-checks `deckGroup`'s sidecar lyrics (same lookup
    /// slotTrackChanged() already does on a real load) without requiring
    /// a new track load -- the trigger for this is a background AI
    /// transcription job (WhisperTranscriptionManager) finishing and
    /// writing a new .lrc file for a track that may already be loaded and
    /// playing, which wouldn't otherwise produce a PlayerInfo::
    /// trackChanged signal to react to. A no-op if this deck's currently
    /// loaded track doesn't match what's playing anymore.
    void reloadLyricsForDeck(const QString& deckGroup);

  signals:
    void karaokeModeEnabledChanged();

  private slots:
    void slotTrackChanged(const QString& group, TrackPointer pNewTrack, TrackPointer pOldTrack);

  private:
    void loadLyricsFor(const QString& deckGroup, const QString& trackFilePath);
    /// M14 Stage 4: when karaoke mode is on and a lyrics source was just
    /// found for this deck, triggers EngineDeck's existing
    /// "stem_instrumental" ControlPushButton (mutes the vocal stem only --
    /// built for M4/M8's Instrumental combo pad, reused here verbatim) --
    /// a no-op if the loaded track has no stems (the CO simply won't
    /// exist/won't be valid, same as any other ControlProxy on a
    /// non-existent control).
    void maybeAutoMuteVocals(const QString& deckGroup);

    QHash<QString, std::shared_ptr<LyricsSource>> m_sourceByDeck;
    bool m_karaokeModeEnabled = false;
};

} // namespace mixxx
