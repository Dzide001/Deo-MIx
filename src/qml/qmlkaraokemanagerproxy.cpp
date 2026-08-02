#include "qml/qmlkaraokemanagerproxy.h"

#include "moc_qmlkaraokemanagerproxy.cpp"
#include "qml/qmlconfigproxy.h"

namespace mixxx {
namespace qml {

namespace {
const ConfigKey kKaraokeDisplayScreenIndexKey =
        ConfigKey(QStringLiteral("[Karaoke]"), QStringLiteral("DisplayScreenIndex"));
} // namespace

QmlKaraokeManagerProxy::QmlKaraokeManagerProxy(QObject* parent)
        : QObject(parent),
          m_pSingerQueue(new QmlSingerQueueModel(this)) {
    // Restore the persisted second-screen choice. Defaults to 0 (the
    // primary screen) both when never set and when the saved index no
    // longer resolves -- KaraokeDisplayWindow.qml separately range-checks
    // against the CURRENT screen list too, so a monitor unplugged since
    // last run degrades to the primary screen rather than an invalid
    // window geometry.
    auto pConfig = QmlConfigProxy::get();
    if (pConfig) {
        m_karaokeDisplayScreenIndex =
                pConfig->getValue(kKaraokeDisplayScreenIndexKey, 0);
    }
    // s_pKaraokeManager is seeded from qmlapplication.cpp before the QML
    // engine ever loads (and therefore before any QML_SINGLETON like this
    // one can be lazily constructed), so it's always already valid here.
    if (s_pKaraokeManager) {
        connect(s_pKaraokeManager.get(),
                &mixxx::KaraokeManager::karaokeModeEnabledChanged,
                this,
                &QmlKaraokeManagerProxy::karaokeModeEnabledChanged);
    }
}

// static
QmlKaraokeManagerProxy* QmlKaraokeManagerProxy::create(
        QQmlEngine* pQmlEngine, [[maybe_unused]] QJSEngine* pJsEngine) {
    return new QmlKaraokeManagerProxy(pQmlEngine);
}

void QmlKaraokeManagerProxy::setKaraokeDisplayScreenIndex(int index) {
    if (m_karaokeDisplayScreenIndex == index) {
        return;
    }
    m_karaokeDisplayScreenIndex = index;
    auto pConfig = QmlConfigProxy::get();
    if (pConfig) {
        pConfig->setValue(kKaraokeDisplayScreenIndexKey, index);
    }
    emit karaokeDisplayScreenIndexChanged();
}

bool QmlKaraokeManagerProxy::hasLyrics(const QString& deckGroup) const {
    if (!s_pKaraokeManager) {
        return false;
    }
    return s_pKaraokeManager->hasLyrics(deckGroup);
}

QString QmlKaraokeManagerProxy::currentLine(const QString& deckGroup, double positionSeconds) const {
    if (!s_pKaraokeManager) {
        return QString();
    }
    return s_pKaraokeManager->currentLine(deckGroup, positionSeconds);
}

int QmlKaraokeManagerProxy::currentWordIndex(const QString& deckGroup, double positionSeconds) const {
    if (!s_pKaraokeManager) {
        return -1;
    }
    return s_pKaraokeManager->currentWordIndex(deckGroup, positionSeconds);
}

bool QmlKaraokeManagerProxy::hasCdgSource(const QString& deckGroup) const {
    if (!s_pKaraokeManager) {
        return false;
    }
    return s_pKaraokeManager->hasCdgSource(deckGroup);
}

bool QmlKaraokeManagerProxy::isKaraokeModeEnabled() const {
    return s_pKaraokeManager && s_pKaraokeManager->isKaraokeModeEnabled();
}

void QmlKaraokeManagerProxy::setKaraokeModeEnabled(bool enabled) {
    if (s_pKaraokeManager) {
        s_pKaraokeManager->setKaraokeModeEnabled(enabled);
    }
}

} // namespace qml
} // namespace mixxx
