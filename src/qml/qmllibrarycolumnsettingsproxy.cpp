#include "qml/qmllibrarycolumnsettingsproxy.h"

#include "moc_qmllibrarycolumnsettingsproxy.cpp"
#include "preferences/librarycolumnsettings.h"

namespace mixxx {
namespace qml {

QmlLibraryColumnSettingsProxy::QmlLibraryColumnSettingsProxy(QObject* pParent)
        : QObject(pParent) {
}

bool QmlLibraryColumnSettingsProxy::isColumnHidden(int columnIdx, bool defaultHidden) const {
    LibraryColumnSettings settings(s_pUserSettings);
    return settings.isColumnHidden(columnIdx, defaultHidden);
}

void QmlLibraryColumnSettingsProxy::setColumnHidden(int columnIdx, bool hidden) {
    LibraryColumnSettings settings(s_pUserSettings);
    settings.setColumnHidden(columnIdx, hidden);
}

// static
QmlLibraryColumnSettingsProxy* QmlLibraryColumnSettingsProxy::create(
        QQmlEngine* pQmlEngine, QJSEngine*) {
    // Same pattern as QmlCustomPadSettingsProxy::create()/
    // QmlConfigProxy::create() -- the code example Qt's own docs show for
    // QML_SINGLETON replacing qmlRegisterSingletonInstance().
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON
    VERIFY_OR_DEBUG_ASSERT(s_pUserSettings) {
        qWarning() << "UserSettings hasn't been registered yet";
        return nullptr;
    }
    return new QmlLibraryColumnSettingsProxy(pQmlEngine);
}

} // namespace qml
} // namespace mixxx
