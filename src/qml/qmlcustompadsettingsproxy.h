#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include "preferences/usersettings.h"

namespace mixxx {
namespace qml {

// QML-facing wrapper around CustomPadSettings, following the exact
// QML_SINGLETON registration pattern QmlConfigProxy already uses
// (static create()/registerUserSettings(), wired up in
// CoreServices::initializeQMLSingletons()/finalize()). No Q_PROPERTY/
// NOTIFY machinery is needed here (unlike QmlConfigProxy) -- a pad's
// assignment only ever changes through direct, local, imperative user
// action inside CustomPad.qml itself, which re-reads immediately after
// calling setPadAssignment(), rather than through any externally-bound
// live consumer elsewhere in the skin.
class QmlCustomPadSettingsProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(CustomPadSettings)
    QML_SINGLETON

  public:
    explicit QmlCustomPadSettingsProxy(QObject* pParent = nullptr);

    Q_INVOKABLE QString getPadGroup(const QString& deckGroup, int padIndex) const;
    Q_INVOKABLE QString getPadKey(const QString& deckGroup, int padIndex) const;
    Q_INVOKABLE QString getPadLabel(const QString& deckGroup, int padIndex) const;
    Q_INVOKABLE bool isPadAssigned(const QString& deckGroup, int padIndex) const;
    Q_INVOKABLE void setPadAssignment(const QString& deckGroup,
            int padIndex,
            const QString& group,
            const QString& key,
            const QString& label);
    Q_INVOKABLE void clearPadAssignment(const QString& deckGroup, int padIndex);

    static QmlCustomPadSettingsProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static inline void registerUserSettings(UserSettingsPointer pConfig) {
        s_pUserSettings = std::move(pConfig);
    }

  private:
    static inline UserSettingsPointer s_pUserSettings = nullptr;
};

} // namespace qml
} // namespace mixxx
