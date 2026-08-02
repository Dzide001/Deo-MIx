#pragma once

#include <QObject>
#include <QSet>
#include <QString>

#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "util/parented_ptr.h"

class TrackCollectionManager;
class PlayerManager;
class BaseTrackPlayer;

namespace mixxx {

class StemSeparationJob;

/// Owns the (at most one, for now) in-flight AI stem-separation job and is
/// the entry point for triggering one. Modeled on how LibraryExporter owns
/// and drives an EnginePrimeExportJob
/// (src/library/export/libraryexporter.cpp).
///
/// Also watches every deck for newly-loaded tracks: if a track that was
/// separated before (a matching file already exists under the settings
/// path's stems/ directory) gets loaded again, the deck is silently
/// swapped to that existing stem track -- no re-running AI separation, no
/// user action needed. Tracks never separated before are left alone.
class StemSeparationManager : public QObject {
    Q_OBJECT
  public:
    StemSeparationManager(
            QObject* parent,
            UserSettingsPointer pConfig,
            TrackCollectionManager* pTrackCollectionManager,
            PlayerManager* pPlayerManager);
    ~StemSeparationManager() override;

    /// Starts a background separation job for `pSourceTrack`. On success,
    /// the resulting stem file is registered as a new library track and,
    /// if `deckGroup` still holds `pSourceTrack` when the job finishes,
    /// reloaded into that deck. If a stem file for this exact track was
    /// already generated previously, reuses it immediately instead of
    /// re-running AI separation.
    ///
    /// Returns false (nothing started) if a job is already running, the
    /// HTDemucs model path preference is unset/missing, or `pSourceTrack`
    /// is not a resolved, file-backed library track. In that case `failed`
    /// is emitted synchronously with an explanatory message.
    bool prepareStems(TrackPointer pSourceTrack, const QString& deckGroup);

    bool isRunning() const {
        return static_cast<bool>(m_pCurrentJob);
    }

    /// Requests cancellation of any in-flight job and waits for it to stop.
    /// Called from CoreServices' destructor before TrackCollectionManager/
    /// PlayerManager are torn down.
    void cancelAndWait();

    /// Configured Demucs ONNX model path, empty if unset. Same
    /// [AiStemSeparation]/ModelPath config value the legacy
    /// DlgPrefAiStemSeparation page edits -- exposed here so the QML
    /// settings page can edit it too without duplicating the key.
    QString modelPath() const {
        return resolveModelPath();
    }
    void setModelPath(const QString& path);

  signals:
    void progressChanged(float fraction, const QString& message);
    void finished(TrackPointer pNewStemTrack);
    void failed(const QString& message);

  private:
    QString resolveModelPath() const;
    QString resolveOutputPath(const TrackPointer& pSourceTrack) const;
    bool tryReuseExistingSeparation(
            const TrackPointer& pSourceTrack,
            const QString& outputPath,
            const QString& deckGroup);

    void connectDeckForAutoLoad(const QString& deckGroup);
    void slotTrackLoadedOnDeck(TrackPointer pTrack, const QString& deckGroup);

    UserSettingsPointer m_pConfig;
    TrackCollectionManager* m_pTrackCollectionManager;
    PlayerManager* m_pPlayerManager;
    parented_ptr<StemSeparationJob> m_pCurrentJob;
    QSet<QString> m_autoLoadConnectedGroups;
};

} // namespace mixxx
