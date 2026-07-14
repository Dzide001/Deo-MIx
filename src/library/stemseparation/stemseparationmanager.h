#pragma once

#include <QObject>
#include <QString>

#include "preferences/usersettings.h"
#include "track/track_decl.h"
#include "util/parented_ptr.h"

class TrackCollectionManager;
class PlayerManager;

namespace mixxx {

class StemSeparationJob;

/// Owns the (at most one, for now) in-flight AI stem-separation job and is
/// the entry point for triggering one. Modeled on how LibraryExporter owns
/// and drives an EnginePrimeExportJob
/// (src/library/export/libraryexporter.cpp).
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
    /// reloaded into that deck.
    ///
    /// Returns false (no job started) if a job is already running, the
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

  signals:
    void progressChanged(float fraction, const QString& message);
    void finished(TrackPointer pNewStemTrack);
    void failed(const QString& message);

  private:
    QString resolveModelPath() const;
    QString resolveOutputPath(const TrackPointer& pSourceTrack) const;

    UserSettingsPointer m_pConfig;
    TrackCollectionManager* m_pTrackCollectionManager;
    PlayerManager* m_pPlayerManager;
    parented_ptr<StemSeparationJob> m_pCurrentJob;
};

} // namespace mixxx
