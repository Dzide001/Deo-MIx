#include "qml/qmlcustompadsettingsproxy.h"

#include "moc_qmlcustompadsettingsproxy.cpp"
#include "preferences/custompadsettings.h"

namespace mixxx {
namespace qml {

QmlCustomPadSettingsProxy::QmlCustomPadSettingsProxy(QObject* pParent)
        : QObject(pParent) {
}

QString QmlCustomPadSettingsProxy::getPadGroup(const QString& deckGroup, int padIndex) const {
    CustomPadSettings settings(s_pUserSettings);
    return settings.getPadGroup(deckGroup, padIndex);
}

QString QmlCustomPadSettingsProxy::getPadKey(const QString& deckGroup, int padIndex) const {
    CustomPadSettings settings(s_pUserSettings);
    return settings.getPadKey(deckGroup, padIndex);
}

QString QmlCustomPadSettingsProxy::getPadLabel(const QString& deckGroup, int padIndex) const {
    CustomPadSettings settings(s_pUserSettings);
    return settings.getPadLabel(deckGroup, padIndex);
}

bool QmlCustomPadSettingsProxy::isPadAssigned(const QString& deckGroup, int padIndex) const {
    CustomPadSettings settings(s_pUserSettings);
    return settings.isPadAssigned(deckGroup, padIndex);
}

void QmlCustomPadSettingsProxy::setPadAssignment(const QString& deckGroup,
        int padIndex,
        const QString& group,
        const QString& key,
        const QString& label) {
    CustomPadSettings settings(s_pUserSettings);
    settings.setPadAssignment(deckGroup, padIndex, group, key, label);
}

void QmlCustomPadSettingsProxy::clearPadAssignment(const QString& deckGroup, int padIndex) {
    CustomPadSettings settings(s_pUserSettings);
    settings.clearPadAssignment(deckGroup, padIndex);
}

// static
QmlCustomPadSettingsProxy* QmlCustomPadSettingsProxy::create(
        QQmlEngine* pQmlEngine, QJSEngine*) {
    // Same pattern as QmlConfigProxy::create() -- the code example Qt's own
    // docs show for QML_SINGLETON replacing qmlRegisterSingletonInstance().
    // https://doc.qt.io/qt-6/qqmlengine.html#QML_SINGLETON
    VERIFY_OR_DEBUG_ASSERT(s_pUserSettings) {
        qWarning() << "UserSettings hasn't been registered yet";
        return nullptr;
    }
    return new QmlCustomPadSettingsProxy(pQmlEngine);
}

} // namespace qml
} // namespace mixxx
