#include "qml/qmlstemseparationproxy.h"

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
