#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include "network/jsonwebtask.h"
#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "util/parented_ptr.h"

namespace mixxx {

class LrcLibGetLyricsTask;
class WhisperTranscriptionJob;

/// Owns the (at most one, for now) in-flight lyric-acquisition job and is
/// the entry point for triggering one. Two-stage strategy: first tries a
/// free, no-auth lookup against lrclib.net's community-submitted .lrc
/// database (LrcLibGetLyricsTask) -- exact, human-transcribed lyrics with
/// none of AI transcription's inherent limitations (overlapping vocals,
/// word-timing drift) when a match exists -- and only falls back to local
/// AI transcription (WhisperTranscriptionJob) if lrclib has no match.
/// Modeled on StemSeparationManager (src/library/stemseparation/) for the
/// overall shape, but simpler: no library registration/deck-reload/auto-
/// load-on-deck-watching -- either stage just writes a plain .lrc file to
/// disk, and `finished` is wired (in CoreServices, alongside where both
/// managers are constructed) directly to KaraokeManager::
/// reloadLyricsForDeck() rather than this manager needing a
/// KaraokeManager pointer of its own.
class WhisperTranscriptionManager : public QObject {
    Q_OBJECT
  public:
    WhisperTranscriptionManager(QObject* parent, UserSettingsPointer pConfig);
    ~WhisperTranscriptionManager() override;

    /// Starts lyric acquisition for `pSourceTrack`: first an online
    /// lrclib.net lookup, falling back to local AI transcription if that
    /// finds no match. Either way the result is written to the same .lrc
    /// sidecar path KaraokeManager's own sidecar-detection logic already
    /// looks for (same base filename, same folder as the track).
    ///
    /// Returns false (nothing started) if a job is already running or
    /// `pSourceTrack` is not a resolved, file-backed library track. In
    /// that case `failed` is emitted synchronously with an explanatory
    /// message. If the lrclib lookup itself finds no match, the AI
    /// fallback may still separately fail (e.g. whisper model path
    /// unset) -- that failure is reported asynchronously via `failed`
    /// instead.
    bool transcribeLyrics(TrackPointer pSourceTrack, const QString& deckGroup);

    bool isRunning() const {
        return static_cast<bool>(m_pLookupTask) || static_cast<bool>(m_pCurrentJob);
    }

    /// Requests cancellation of any in-flight lookup/job and waits for it
    /// to stop. Called from CoreServices' destructor before
    /// TrackCollectionManager/PlayerManager are torn down.
    void cancelAndWait();

    /// Configured whisper.cpp GGML model path, empty if unset. Same
    /// [AiLyricTranscription]/ModelPath config value the legacy
    /// DlgPrefAiLyricTranscription page edits -- exposed here so the QML
    /// settings page can edit it too without duplicating the key.
    QString modelPath() const {
        return resolveModelPath();
    }
    void setModelPath(const QString& path);

  signals:
    void progressChanged(float fraction, const QString& message);
    void finished(const QString& deckGroup, const QString& lrcPath);
    void failed(const QString& message);

  private slots:
    void slotLookupSucceeded(const QString& syncedLyrics);
    void slotLookupFailed(const mixxx::network::JsonWebResponse& response);
    void slotLookupNetworkError(QNetworkReply::NetworkError errorCode,
            const QString& errorString,
            const mixxx::network::WebResponseWithContent& responseWithContent);
    void slotLookupAborted(const QUrl& requestUrl);

  private:
    QString resolveModelPath() const;
    /// Disconnects/deletes m_pLookupTask, per NetworkTask's own contract
    /// that a connected receiver is responsible for its cleanup.
    void terminateLookupTask();
    /// The AI-transcription stage, started after the lrclib lookup
    /// concludes without a usable match (m_pPendingTrack/
    /// m_pendingDeckGroup, set by transcribeLyrics() before the lookup
    /// began, are still valid at this point).
    void startWhisperFallback();
    bool writeLrcSidecar(const QString& outputLrcPath, const QString& lrcContents);

    UserSettingsPointer m_pConfig;
    QNetworkAccessManager m_network;
    parented_ptr<LrcLibGetLyricsTask> m_pLookupTask;
    parented_ptr<WhisperTranscriptionJob> m_pCurrentJob;

    // Context for the in-flight lookup/fallback, valid from
    // transcribeLyrics() until either stage concludes.
    TrackPointer m_pPendingTrack;
    QString m_pendingDeckGroup;
};

} // namespace mixxx
