#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>

#include "preferences/broadcastsettings.h"

namespace mixxx {
namespace qml {

// Rough-sketch QML settings proxy for M11 (Broadcast/Streaming). Wraps the
// existing BroadcastSettings/BroadcastProfile C++ model (already fully
// built and used by the legacy DlgPrefBroadcast dialog) rather than
// reinventing it. Only edits the first/default profile -- BroadcastSettings
// always has at least one (BroadcastSettings::loadProfiles() creates a
// default profile if none exist yet), so this is never null in practice.
// Multi-profile add/rename/remove management is out of scope for this pass,
// same as the legacy dialog's BroadcastSettingsModel staging layer isn't
// replicated here -- edits write straight to the profile, committed on
// commit().
class QmlBroadcastProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Broadcast)
    QML_SINGLETON
  public:
    explicit QmlBroadcastProxy(BroadcastSettingsPointer pSettings, QObject* parent = nullptr);
    ~QmlBroadcastProxy() override = default;

    static QmlBroadcastProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    static inline BroadcastSettingsPointer s_pBroadcastSettings;

    Q_INVOKABLE QString getHost() const;
    Q_INVOKABLE void setHost(const QString& value);
    Q_INVOKABLE int getPort() const;
    Q_INVOKABLE void setPort(int value);
    Q_INVOKABLE QString getServerType() const;
    Q_INVOKABLE void setServerType(const QString& value);
    Q_INVOKABLE QString getMountpoint() const;
    Q_INVOKABLE void setMountpoint(const QString& value);
    Q_INVOKABLE QString getLogin() const;
    Q_INVOKABLE void setLogin(const QString& value);
    Q_INVOKABLE QString getPassword() const;
    Q_INVOKABLE void setPassword(const QString& value);
    Q_INVOKABLE QString getFormat() const;
    Q_INVOKABLE void setFormat(const QString& value);
    Q_INVOKABLE int getBitrate() const;
    Q_INVOKABLE void setBitrate(int value);
    Q_INVOKABLE QString getStreamName() const;
    Q_INVOKABLE void setStreamName(const QString& value);
    Q_INVOKABLE QString getStreamDesc() const;
    Q_INVOKABLE void setStreamDesc(const QString& value);
    Q_INVOKABLE QString getStreamGenre() const;
    Q_INVOKABLE void setStreamGenre(const QString& value);
    Q_INVOKABLE bool getStreamPublic() const;
    Q_INVOKABLE void setStreamPublic(bool value);
    Q_INVOKABLE bool getEnableReconnect() const;
    Q_INVOKABLE void setEnableReconnect(bool value);
    Q_INVOKABLE double getReconnectPeriod() const;
    Q_INVOKABLE void setReconnectPeriod(double value);
    Q_INVOKABLE bool getLimitReconnects() const;
    Q_INVOKABLE void setLimitReconnects(bool value);
    Q_INVOKABLE int getMaximumRetries() const;
    Q_INVOKABLE void setMaximumRetries(int value);
    Q_INVOKABLE bool getEnableMetadata() const;
    Q_INVOKABLE void setEnableMetadata(bool value);
    Q_INVOKABLE QString getCustomArtist() const;
    Q_INVOKABLE void setCustomArtist(const QString& value);
    Q_INVOKABLE QString getCustomTitle() const;
    Q_INVOKABLE void setCustomTitle(const QString& value);
    Q_INVOKABLE QString getMetadataFormat() const;
    Q_INVOKABLE void setMetadataFormat(const QString& value);

    // Persists the in-memory profile edits to disk (mirrors
    // QmlSoundManagerProxy::commit()'s fire-and-report-via-signal shape).
    Q_INVOKABLE void commit();

  signals:
    void committed(const QString& error);

  private:
    BroadcastSettingsPointer m_pSettings;
    BroadcastProfilePtr m_pProfile;
};

} // namespace qml
} // namespace mixxx
