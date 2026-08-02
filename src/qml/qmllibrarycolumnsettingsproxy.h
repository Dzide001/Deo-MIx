#pragma once

#include <QObject>
#include <QQmlEngine>

#include "preferences/usersettings.h"

namespace mixxx {
namespace qml {

// QML-facing wrapper around LibraryColumnSettings, following the exact
// QML_SINGLETON registration pattern QmlCustomPadSettingsProxy/
// QmlConfigProxy already use (static create()/registerUserSettings(),
// wired up in CoreServices::initializeQMLSingletons()/destructor). No
// Q_PROPERTY/NOTIFY machinery is needed -- a column's hidden state only
// ever changes through direct, local, imperative user action (the header
// context menu), which sets `hidden` directly on the live
// QmlLibraryTrackListColumn object at the same time it calls
// setColumnHidden() here, rather than through any externally-bound live
// consumer elsewhere in the skin.
class QmlLibraryColumnSettingsProxy : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryColumnSettings)
    QML_SINGLETON

  public:
    explicit QmlLibraryColumnSettingsProxy(QObject* pParent = nullptr);

    Q_INVOKABLE bool isColumnHidden(int columnIdx, bool defaultHidden) const;
    Q_INVOKABLE void setColumnHidden(int columnIdx, bool hidden);

    static QmlLibraryColumnSettingsProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);
    static inline void registerUserSettings(UserSettingsPointer pConfig) {
        s_pUserSettings = std::move(pConfig);
    }

  private:
    static inline UserSettingsPointer s_pUserSettings = nullptr;
};

} // namespace qml
} // namespace mixxx
