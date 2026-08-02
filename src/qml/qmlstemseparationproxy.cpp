#include "qml/qmlstemseparationproxy.h"

#include <QFileInfo>

#include <utility>

#include "library/stemseparation/stemseparationmanager.h"
#include "moc_qmlstemseparationproxy.cpp"
#include "qml/qmltrackproxy.h"
#include "track/track.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

QmlStemSeparationProxy::QmlStemSeparationProxy(
        std::shared_ptr<mixxx::StemSeparationManager> pManager,
        QObject* parent)
        : QObject(parent),
          m_pManager(std::move(pManager)) {
    if (!m_pManager) {
        return;
    }

    connect(m_pManager.get(),
            &mixxx::StemSeparationManager::progressChanged,
            this,
            &QmlStemSeparationProxy::slotProgressChanged);
    connect(m_pManager.get(),
            &mixxx::StemSeparationManager::finished,
            this,
            &QmlStemSeparationProxy::slotFinished);
    connect(m_pManager.get(),
            &mixxx::StemSeparationManager::failed,
            this,
            &QmlStemSeparationProxy::slotFailed);
}

QmlStemSeparationProxy* QmlStemSeparationProxy::create(
        QQmlEngine* pQmlEngine, [[maybe_unused]] QJSEngine* pJsEngine) {
    return new QmlStemSeparationProxy(s_pStemSeparationManager, pQmlEngine);
}

QString QmlStemSeparationProxy::modelPath() const {
    if (!m_pManager) {
        return QString();
    }
    return m_pManager->modelPath();
}

void QmlStemSeparationProxy::setModelPath(const QString& path) {
    if (!m_pManager || m_pManager->modelPath() == path) {
        return;
    }
    m_pManager->setModelPath(path);
    emit modelPathChanged();
}

void QmlStemSeparationProxy::setModelPathFromUrl(const QUrl& url) {
    setModelPath(url.toLocalFile());
}

QString QmlStemSeparationProxy::modelStatus() const {
    const QString path = modelPath();
    if (path.isEmpty()) {
        return tr("Not set");
    }
    return QFileInfo::exists(path) ? tr("Found") : tr("File missing!");
}

bool QmlStemSeparationProxy::prepareStems(
        const QmlTrackProxy* track, const QString& deckGroup) {
    VERIFY_OR_DEBUG_ASSERT(track && track->internal() && m_pManager) {
        return false;
    }

    const bool started = m_pManager->prepareStems(track->internal(), deckGroup);
    if (started) {
        m_isRunning = true;
        emit runningChanged();
    }
    return started;
}

void QmlStemSeparationProxy::slotProgressChanged(float fraction, const QString& message) {
    Q_UNUSED(message);
    m_progress = fraction;
    emit progressChanged();
}

void QmlStemSeparationProxy::slotFinished(TrackPointer pNewStemTrack) {
    Q_UNUSED(pNewStemTrack);
    m_isRunning = false;
    emit runningChanged();
}

void QmlStemSeparationProxy::slotFailed(const QString& message) {
    m_isRunning = false;
    emit runningChanged();
    emit failed(message);
}

} // namespace qml
} // namespace mixxx
