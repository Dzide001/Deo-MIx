#include "qml/qmlbroadcastproxy.h"

#include <utility>

#include "moc_qmlbroadcastproxy.cpp"
#include "preferences/broadcastprofile.h"

namespace mixxx {
namespace qml {

QmlBroadcastProxy::QmlBroadcastProxy(BroadcastSettingsPointer pSettings, QObject* parent)
        : QObject(parent),
          m_pSettings(std::move(pSettings)),
          m_pProfile(m_pSettings ? m_pSettings->profileAt(0) : nullptr) {
}

QmlBroadcastProxy* QmlBroadcastProxy::create(
        QQmlEngine* pQmlEngine, [[maybe_unused]] QJSEngine* pJsEngine) {
    return new QmlBroadcastProxy(s_pBroadcastSettings, pQmlEngine);
}

QString QmlBroadcastProxy::getHost() const {
    return m_pProfile ? m_pProfile->getHost() : QString();
}

void QmlBroadcastProxy::setHost(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setHost(value);
    }
}

int QmlBroadcastProxy::getPort() const {
    return m_pProfile ? m_pProfile->getPort() : 0;
}

void QmlBroadcastProxy::setPort(int value) {
    if (m_pProfile) {
        m_pProfile->setPort(value);
    }
}

QString QmlBroadcastProxy::getServerType() const {
    return m_pProfile ? m_pProfile->getServertype() : QString();
}

void QmlBroadcastProxy::setServerType(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setServertype(value);
    }
}

QString QmlBroadcastProxy::getMountpoint() const {
    return m_pProfile ? m_pProfile->getMountpoint() : QString();
}

void QmlBroadcastProxy::setMountpoint(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setMountPoint(value);
    }
}

QString QmlBroadcastProxy::getLogin() const {
    return m_pProfile ? m_pProfile->getLogin() : QString();
}

void QmlBroadcastProxy::setLogin(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setLogin(value);
    }
}

QString QmlBroadcastProxy::getPassword() const {
    return m_pProfile ? m_pProfile->getPassword() : QString();
}

void QmlBroadcastProxy::setPassword(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setPassword(value);
    }
}

QString QmlBroadcastProxy::getFormat() const {
    return m_pProfile ? m_pProfile->getFormat() : QString();
}

void QmlBroadcastProxy::setFormat(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setFormat(value);
    }
}

int QmlBroadcastProxy::getBitrate() const {
    return m_pProfile ? m_pProfile->getBitrate() : 0;
}

void QmlBroadcastProxy::setBitrate(int value) {
    if (m_pProfile) {
        m_pProfile->setBitrate(value);
    }
}

QString QmlBroadcastProxy::getStreamName() const {
    return m_pProfile ? m_pProfile->getStreamName() : QString();
}

void QmlBroadcastProxy::setStreamName(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setStreamName(value);
    }
}

QString QmlBroadcastProxy::getStreamDesc() const {
    return m_pProfile ? m_pProfile->getStreamDesc() : QString();
}

void QmlBroadcastProxy::setStreamDesc(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setStreamDesc(value);
    }
}

QString QmlBroadcastProxy::getStreamGenre() const {
    return m_pProfile ? m_pProfile->getStreamGenre() : QString();
}

void QmlBroadcastProxy::setStreamGenre(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setStreamGenre(value);
    }
}

bool QmlBroadcastProxy::getStreamPublic() const {
    return m_pProfile && m_pProfile->getStreamPublic();
}

void QmlBroadcastProxy::setStreamPublic(bool value) {
    if (m_pProfile) {
        m_pProfile->setStreamPublic(value);
    }
}

bool QmlBroadcastProxy::getEnableReconnect() const {
    return m_pProfile && m_pProfile->getEnableReconnect();
}

void QmlBroadcastProxy::setEnableReconnect(bool value) {
    if (m_pProfile) {
        m_pProfile->setEnableReconnect(value);
    }
}

double QmlBroadcastProxy::getReconnectPeriod() const {
    return m_pProfile ? m_pProfile->getReconnectPeriod() : 0.0;
}

void QmlBroadcastProxy::setReconnectPeriod(double value) {
    if (m_pProfile) {
        m_pProfile->setReconnectPeriod(value);
    }
}

bool QmlBroadcastProxy::getLimitReconnects() const {
    return m_pProfile && m_pProfile->getLimitReconnects();
}

void QmlBroadcastProxy::setLimitReconnects(bool value) {
    if (m_pProfile) {
        m_pProfile->setLimitReconnects(value);
    }
}

int QmlBroadcastProxy::getMaximumRetries() const {
    return m_pProfile ? m_pProfile->getMaximumRetries() : 0;
}

void QmlBroadcastProxy::setMaximumRetries(int value) {
    if (m_pProfile) {
        m_pProfile->setMaximumRetries(value);
    }
}

bool QmlBroadcastProxy::getEnableMetadata() const {
    return m_pProfile && m_pProfile->getEnableMetadata();
}

void QmlBroadcastProxy::setEnableMetadata(bool value) {
    if (m_pProfile) {
        m_pProfile->setEnableMetadata(value);
    }
}

QString QmlBroadcastProxy::getCustomArtist() const {
    return m_pProfile ? m_pProfile->getCustomArtist() : QString();
}

void QmlBroadcastProxy::setCustomArtist(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setCustomArtist(value);
    }
}

QString QmlBroadcastProxy::getCustomTitle() const {
    return m_pProfile ? m_pProfile->getCustomTitle() : QString();
}

void QmlBroadcastProxy::setCustomTitle(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setCustomTitle(value);
    }
}

QString QmlBroadcastProxy::getMetadataFormat() const {
    return m_pProfile ? m_pProfile->getMetadataFormat() : QString();
}

void QmlBroadcastProxy::setMetadataFormat(const QString& value) {
    if (m_pProfile) {
        m_pProfile->setMetadataFormat(value);
    }
}

void QmlBroadcastProxy::commit() {
    if (!m_pSettings || !m_pProfile) {
        emit committed(QStringLiteral("No broadcast profile available"));
        return;
    }
    if (!m_pSettings->saveProfile(m_pProfile.data())) {
        emit committed(QStringLiteral("Failed to save broadcast profile"));
        return;
    }
    emit committed(QString());
}

} // namespace qml
} // namespace mixxx
