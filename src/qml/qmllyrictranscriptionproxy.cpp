#include "qml/qmllyrictranscriptionproxy.h"

#include <QFileInfo>

#include <utility>

#include "library/lyrictranscription/whispertranscriptionmanager.h"
#include "moc_qmllyrictranscriptionproxy.cpp"
#include "qml/qmltrackproxy.h"
#include "track/track.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

QmlLyricTranscriptionProxy::QmlLyricTranscriptionProxy(
        std::shared_ptr<mixxx::WhisperTranscriptionManager> pManager,
        QObject* parent)
        : QObject(parent),
          m_pManager(std::move(pManager)) {
    if (!m_pManager) {
        return;
    }

    connect(m_pManager.get(),
            &mixxx::WhisperTranscriptionManager::progressChanged,
            this,
            &QmlLyricTranscriptionProxy::slotProgressChanged);
    connect(m_pManager.get(),
            &mixxx::WhisperTranscriptionManager::finished,
            this,
            &QmlLyricTranscriptionProxy::slotFinished);
    connect(m_pManager.get(),
            &mixxx::WhisperTranscriptionManager::failed,
            this,
            &QmlLyricTranscriptionProxy::slotFailed);
}

QmlLyricTranscriptionProxy* QmlLyricTranscriptionProxy::create(
        QQmlEngine* pQmlEngine, [[maybe_unused]] QJSEngine* pJsEngine) {
    return new QmlLyricTranscriptionProxy(s_pWhisperTranscriptionManager, pQmlEngine);
}

QString QmlLyricTranscriptionProxy::modelPath() const {
    if (!m_pManager) {
        return QString();
    }
    return m_pManager->modelPath();
}

void QmlLyricTranscriptionProxy::setModelPath(const QString& path) {
    if (!m_pManager || m_pManager->modelPath() == path) {
        return;
    }
    m_pManager->setModelPath(path);
    emit modelPathChanged();
}

void QmlLyricTranscriptionProxy::setModelPathFromUrl(const QUrl& url) {
    setModelPath(url.toLocalFile());
}

QString QmlLyricTranscriptionProxy::modelStatus() const {
    const QString path = modelPath();
    if (path.isEmpty()) {
        return tr("Not set");
    }
    return QFileInfo::exists(path) ? tr("Found") : tr("File missing!");
}

bool QmlLyricTranscriptionProxy::transcribeLyrics(
        const QmlTrackProxy* track, const QString& deckGroup) {
    VERIFY_OR_DEBUG_ASSERT(track && track->internal() && m_pManager) {
        return false;
    }

    const bool started = m_pManager->transcribeLyrics(track->internal(), deckGroup);
    if (started) {
        m_isRunning = true;
        emit runningChanged();
    }
    return started;
}

void QmlLyricTranscriptionProxy::slotProgressChanged(float fraction, const QString& message) {
    m_progress = fraction;
    m_statusMessage = message;
    emit progressChanged();
}

void QmlLyricTranscriptionProxy::slotFinished(const QString& deckGroup, const QString& lrcPath) {
    Q_UNUSED(deckGroup);
    Q_UNUSED(lrcPath);
    m_isRunning = false;
    emit runningChanged();
}

void QmlLyricTranscriptionProxy::slotFailed(const QString& message) {
    m_isRunning = false;
    emit runningChanged();
    emit failed(message);
}

} // namespace qml
} // namespace mixxx
