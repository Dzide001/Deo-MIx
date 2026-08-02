#include "library/lyrictranscription/whispertranscriptionmanager.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

#include "library/lyrictranscription/defs_lyrictranscription.h"
#include "library/lyrictranscription/lrclibgetlyricstask.h"
#include "library/lyrictranscription/whispertranscriptionjob.h"
#include "moc_whispertranscriptionmanager.cpp"
#include "track/track.h"
#include "util/logger.h"
#include "util/parented_ptr.h"

namespace mixxx {

namespace {

const Logger kLogger("WhisperTranscriptionManager");

constexpr int kLrcLibTimeoutMillis = 8000;

// Same base filename as the track, same folder -- the exact convention
// KaraokeManager::loadLyricsFor() (src/library/karaoke/karaokemanager.cpp)
// already checks for a .lrc sidecar. Duplicated rather than shared: it's
// four lines, and pulling in a dependency on the karaoke module just for
// this one helper isn't worth it.
QString sidecarLrcPathFor(const QString& trackFilePath) {
    const QFileInfo trackFileInfo(trackFilePath);
    return trackFileInfo.absolutePath() + QChar('/') + trackFileInfo.completeBaseName() +
            QStringLiteral(".lrc");
}

} // namespace

WhisperTranscriptionManager::WhisperTranscriptionManager(QObject* parent, UserSettingsPointer pConfig)
        : QObject(parent), m_pConfig(std::move(pConfig)) {
}

WhisperTranscriptionManager::~WhisperTranscriptionManager() {
    cancelAndWait();
}

QString WhisperTranscriptionManager::resolveModelPath() const {
    return m_pConfig->getValueString(ConfigKey(AI_LYRIC_TRANSCRIPTION_PREF_KEY, "ModelPath"));
}

void WhisperTranscriptionManager::setModelPath(const QString& path) {
    m_pConfig->setValue(ConfigKey(AI_LYRIC_TRANSCRIPTION_PREF_KEY, "ModelPath"), path);
}

bool WhisperTranscriptionManager::transcribeLyrics(
        TrackPointer pSourceTrack, const QString& deckGroup) {
    if (isRunning()) {
        emit failed(tr("A lyric lookup/transcription job is already running."));
        return false;
    }
    if (!pSourceTrack || pSourceTrack->getLocation().isEmpty()) {
        emit failed(tr("This track cannot be used for lyric transcription."));
        return false;
    }

    m_pPendingTrack = pSourceTrack;
    m_pendingDeckGroup = deckGroup;

    emit progressChanged(0.02f, tr("Looking up lyrics online..."));

    m_pLookupTask = make_parented<LrcLibGetLyricsTask>(
            &m_network,
            pSourceTrack->getTitle(),
            pSourceTrack->getArtist(),
            pSourceTrack->getAlbum(),
            pSourceTrack->getDuration(),
            this);

    connect(m_pLookupTask.get(),
            &LrcLibGetLyricsTask::succeeded,
            this,
            &WhisperTranscriptionManager::slotLookupSucceeded);
    connect(m_pLookupTask.get(),
            &network::JsonWebTask::failed,
            this,
            &WhisperTranscriptionManager::slotLookupFailed);
    connect(m_pLookupTask.get(),
            &network::WebTask::networkError,
            this,
            &WhisperTranscriptionManager::slotLookupNetworkError);
    connect(m_pLookupTask.get(),
            &network::NetworkTask::aborted,
            this,
            &WhisperTranscriptionManager::slotLookupAborted);

    m_pLookupTask->invokeStart(kLrcLibTimeoutMillis);
    return true;
}

void WhisperTranscriptionManager::terminateLookupTask() {
    if (!m_pLookupTask) {
        return;
    }
    m_pLookupTask->disconnect(this);
    m_pLookupTask->deleteLater();
    m_pLookupTask = nullptr;
}

void WhisperTranscriptionManager::slotLookupSucceeded(const QString& syncedLyrics) {
    if (m_pLookupTask.get() != sender()) {
        return; // stray signal from an already-terminated task
    }
    terminateLookupTask();

    const QString outputLrcPath = sidecarLrcPathFor(m_pPendingTrack->getLocation());
    if (!writeLrcSidecar(outputLrcPath, syncedLyrics)) {
        emit failed(tr("Failed to write %1").arg(outputLrcPath));
        return;
    }
    kLogger.debug() << "Wrote lrclib.net lyrics to" << outputLrcPath;
    emit progressChanged(1.0f, tr("Done"));
    emit finished(m_pendingDeckGroup, outputLrcPath);
}

void WhisperTranscriptionManager::slotLookupFailed(const network::JsonWebResponse& response) {
    Q_UNUSED(response);
    if (m_pLookupTask.get() != sender()) {
        return;
    }
    terminateLookupTask();
    startWhisperFallback();
}

void WhisperTranscriptionManager::slotLookupNetworkError(
        QNetworkReply::NetworkError errorCode,
        const QString& errorString,
        const network::WebResponseWithContent& responseWithContent) {
    Q_UNUSED(errorCode);
    Q_UNUSED(responseWithContent);
    if (m_pLookupTask.get() != sender()) {
        return;
    }
    kLogger.debug() << "lrclib.net lookup failed, falling back to AI transcription:" << errorString;
    terminateLookupTask();
    startWhisperFallback();
}

void WhisperTranscriptionManager::slotLookupAborted(const QUrl& requestUrl) {
    Q_UNUSED(requestUrl);
    if (m_pLookupTask.get() != sender()) {
        return;
    }
    // Explicit cancellation (cancelAndWait()) -- not a "no match" case,
    // so unlike slotLookupFailed()/slotLookupNetworkError() this does not
    // start the AI fallback.
    terminateLookupTask();
}

void WhisperTranscriptionManager::startWhisperFallback() {
    const QString modelPath = resolveModelPath();
    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        emit failed(tr("No online lyrics found, and no whisper model is set for AI "
                        "transcription. Set the model file path in "
                        "Preferences > AI Lyric Transcription."));
        return;
    }

    WhisperTranscriptionRequest request;
    request.pSourceTrack = m_pPendingTrack;
    request.modelPath = modelPath;
    request.outputLrcPath = sidecarLrcPathFor(m_pPendingTrack->getLocation());
    request.deckGroup = m_pendingDeckGroup;

    m_pCurrentJob = make_parented<WhisperTranscriptionJob>(this, request);

    connect(m_pCurrentJob.get(),
            &WhisperTranscriptionJob::jobProgress,
            this,
            &WhisperTranscriptionManager::progressChanged);
    connect(m_pCurrentJob.get(),
            &WhisperTranscriptionJob::completed,
            this,
            &WhisperTranscriptionManager::finished);
    connect(m_pCurrentJob.get(),
            &WhisperTranscriptionJob::failed,
            this,
            &WhisperTranscriptionManager::failed);
    connect(m_pCurrentJob.get(), &QThread::finished, this, [this]() {
        m_pCurrentJob = nullptr;
    });

    m_pCurrentJob->start();
}

bool WhisperTranscriptionManager::writeLrcSidecar(
        const QString& outputLrcPath, const QString& lrcContents) {
    QFile lrcFile(outputLrcPath);
    if (!lrcFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    lrcFile.write(lrcContents.toUtf8());
    lrcFile.close();
    return true;
}

void WhisperTranscriptionManager::cancelAndWait() {
    if (m_pLookupTask) {
        m_pLookupTask->invokeAbort();
        terminateLookupTask();
    }
    if (!m_pCurrentJob) {
        return;
    }
    WhisperTranscriptionJob* pJob = m_pCurrentJob.get();
    pJob->slotCancel();
    while (m_pCurrentJob && !pJob->wait(50)) {
        QCoreApplication::processEvents();
    }
}

} // namespace mixxx
