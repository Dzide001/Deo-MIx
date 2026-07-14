#pragma once

#include <QAtomicInteger>
#include <QString>
#include <QThread>

#include "track/track_decl.h"

class TrackCollectionManager;
class PlayerManager;

namespace mixxx {

/// A request to separate one track's audio into stems and register the
/// result as a new library track.
struct StemSeparationRequest {
    TrackPointer pSourceTrack; // must already be resolved, i.e. has a TrackId
    QString modelPath;         // path to the HTDemucs ONNX model file
    QString outputPath;        // final .stem.mp4 path, see StemSeparationManager
};

/// Runs AI stem separation for one track on a worker thread: reads the
/// source track's full audio, runs HTDemucs ONNX inference
/// (lib/demucsonnx), writes a real .stem.mp4 (stemsep::writeStemFile), then
/// registers the result as a new library track and (optionally) reloads a
/// deck to it. Modeled directly on EnginePrimeExportJob
/// (src/library/export/engineprimeexportjob.h) -- same QThread-subclass,
/// polled-cancellation-flag, blocking-invoke-for-main-thread-work shape.
class StemSeparationJob : public QThread {
    Q_OBJECT
  public:
    StemSeparationJob(
            QObject* parent,
            TrackCollectionManager* pTrackCollectionManager,
            PlayerManager* pPlayerManager,
            const QString& targetDeckGroup, // empty = register only, no reload
            StemSeparationRequest request);
    ~StemSeparationJob() override;

    void run() override;

  signals:
    /// `fraction` is 0.0-1.0.
    void jobProgress(float fraction, const QString& message);
    void completed(TrackPointer pNewStemTrack);
    void failed(const QString& message);

  public slots:
    void slotCancel();

  private slots:
    // Runs on TrackCollectionManager's (main) thread via a blocking
    // Qt::BlockingQueuedConnection invoke from run() -- getOrAddTrack() and
    // PlayerManager::slotLoadTrackToPlayer() both assert main-thread
    // affinity and cannot be called directly from the worker thread.
    void registerAndReload();

  private:
    QAtomicInteger<int> m_cancellationRequested;

    TrackCollectionManager* m_pTrackCollectionManager;
    PlayerManager* m_pPlayerManager;
    QString m_targetDeckGroup;
    StemSeparationRequest m_request;

    // Written by run() on the worker thread before the blocking invoke,
    // read/written by registerAndReload() on the main thread during it --
    // no concurrent access since run() blocks until registerAndReload()
    // returns.
    QString m_writtenStemFilePath;
    TrackPointer m_pRegisteredTrack;
    QString m_lastErrorMessage;
};

} // namespace mixxx
