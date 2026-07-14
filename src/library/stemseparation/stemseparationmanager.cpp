#include "library/stemseparation/stemseparationmanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>

#include "library/stemseparation/defs_stemseparation.h"
#include "library/stemseparation/stemseparationjob.h"
#include "moc_stemseparationmanager.cpp"
#include "track/track.h"
#include "util/parented_ptr.h"

namespace mixxx {

StemSeparationManager::StemSeparationManager(
        QObject* parent,
        UserSettingsPointer pConfig,
        TrackCollectionManager* pTrackCollectionManager,
        PlayerManager* pPlayerManager)
        : QObject(parent),
          m_pConfig(std::move(pConfig)),
          m_pTrackCollectionManager(pTrackCollectionManager),
          m_pPlayerManager(pPlayerManager) {
}

StemSeparationManager::~StemSeparationManager() {
    cancelAndWait();
}

QString StemSeparationManager::resolveModelPath() const {
    return m_pConfig->getValueString(ConfigKey(AI_STEM_SEPARATION_PREF_KEY, "ModelPath"));
}

QString StemSeparationManager::resolveOutputPath(const TrackPointer& pSourceTrack) const {
    const QString stemsDir = m_pConfig->getSettingsPath() + "/stems";
    QDir().mkpath(stemsDir);

    const QByteArray pathHash = QCryptographicHash::hash(
            pSourceTrack->getLocation().toUtf8(), QCryptographicHash::Sha1)
                                         .toHex()
                                         .left(8);
    const QString fileName = pSourceTrack->getId().isValid()
            ? QStringLiteral("%1_%2.stem.mp4")
                      .arg(pSourceTrack->getId().toString(), QString::fromLatin1(pathHash))
            : QStringLiteral("%1.stem.mp4").arg(QString::fromLatin1(pathHash));

    return stemsDir + "/" + fileName;
}

bool StemSeparationManager::prepareStems(TrackPointer pSourceTrack, const QString& deckGroup) {
    if (isRunning()) {
        emit failed(tr("A stem separation job is already running."));
        return false;
    }
    if (!pSourceTrack || !pSourceTrack->getId().isValid() ||
            pSourceTrack->getLocation().isEmpty()) {
        emit failed(tr("This track cannot be used for stem separation."));
        return false;
    }

    const QString modelPath = resolveModelPath();
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        emit failed(tr("HTDemucs model not found. Set the model file path in "
                        "Preferences > AI Stem Separation."));
        return false;
    }

    StemSeparationRequest request;
    request.pSourceTrack = pSourceTrack;
    request.modelPath = modelPath;
    request.outputPath = resolveOutputPath(pSourceTrack);

    m_pCurrentJob = make_parented<StemSeparationJob>(
            this, m_pTrackCollectionManager, m_pPlayerManager, deckGroup, request);

    connect(m_pCurrentJob.get(),
            &StemSeparationJob::jobProgress,
            this,
            &StemSeparationManager::progressChanged);
    connect(m_pCurrentJob.get(),
            &StemSeparationJob::completed,
            this,
            &StemSeparationManager::finished);
    connect(m_pCurrentJob.get(),
            &StemSeparationJob::failed,
            this,
            &StemSeparationManager::failed);
    connect(m_pCurrentJob.get(), &QThread::finished, this, [this]() {
        // The job object itself is deleted by the next connection below;
        // just drop our own reference so isRunning() reflects reality
        // immediately once the thread has stopped.
        m_pCurrentJob = nullptr;
    });
    connect(m_pCurrentJob.get(), &QThread::finished, m_pCurrentJob.get(), &QObject::deleteLater);

    m_pCurrentJob->start();
    return true;
}

void StemSeparationManager::cancelAndWait() {
    if (!m_pCurrentJob) {
        return;
    }
    StemSeparationJob* pJob = m_pCurrentJob.get();
    pJob->slotCancel();
    // A plain QThread::wait() here could deadlock: the worker thread may be
    // blocked inside a Qt::BlockingQueuedConnection invoke onto this (main)
    // thread's event loop (registerAndReload(), see stemseparationjob.cpp),
    // which will never be serviced if this thread stops pumping events.
    // Keep the event loop alive while waiting so that call can complete and
    // the thread can actually reach QThread::finished.
    while (m_pCurrentJob && !pJob->wait(50)) {
        QCoreApplication::processEvents();
    }
}

} // namespace mixxx
