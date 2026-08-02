#pragma once

#include <QAtomicInteger>
#include <QString>
#include <QThread>

#include "track/track_decl.h"

namespace mixxx {

/// A request to transcribe one track's audio into a real .lrc sidecar
/// file next to it.
struct WhisperTranscriptionRequest {
    TrackPointer pSourceTrack; // must already be resolved, i.e. has a TrackId
    QString modelPath; // path to a ggml-*.bin whisper.cpp model file
    QString outputLrcPath; // KaraokeManager's own sidecar path for this track
    QString deckGroup; // which deck to notify once the .lrc file is ready
};

/// Runs AI lyric transcription for one track on a worker thread: reads
/// the source track's full audio, downmixes/resamples it to whisper.cpp's
/// required 16kHz mono input, runs word-level-timestamped transcription,
/// groups the resulting words into lines, and writes a real enhanced-LRC
/// .lrc file to KaraokeManager's existing sidecar path -- no changes
/// needed to anything M14 already built to consume it (LrcParser,
/// LrcLyricsSource, LyricDisplay.qml, KaraokeDisplayWindow.qml all
/// already parse and render whatever this produces).
///
/// Modeled on StemSeparationJob (src/library/stemseparation/) for the
/// overall shape (QThread subclass, polled QAtomicInteger cancellation
/// flag, jobProgress/completed/failed signals) but simpler: writing a
/// plain .lrc file is safe from any thread (unlike StemSeparationJob's
/// registerAndReload(), which needs a blocking main-thread invoke to
/// safely touch TrackCollectionManager) -- run() does everything itself
/// and emits completed() directly; Qt's default queued connection
/// already marshals that onto whichever thread WhisperTranscriptionManager
/// lives on (the main thread), no explicit blocking invoke needed.
class WhisperTranscriptionJob : public QThread {
    Q_OBJECT
  public:
    WhisperTranscriptionJob(QObject* parent, WhisperTranscriptionRequest request);
    ~WhisperTranscriptionJob() override;

    void run() override;

  signals:
    /// `fraction` is 0.0-1.0.
    void jobProgress(float fraction, const QString& message);
    void completed(const QString& deckGroup, const QString& lrcPath);
    void failed(const QString& message);

  public slots:
    void slotCancel();

  private:
    QAtomicInteger<int> m_cancellationRequested;
    WhisperTranscriptionRequest m_request;
};

} // namespace mixxx
