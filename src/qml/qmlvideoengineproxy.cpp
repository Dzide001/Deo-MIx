#include "qml/qmlvideoengineproxy.h"

#include "control/controlproxy.h"
#include "moc_qmlvideoengineproxy.cpp"

namespace mixxx {
namespace qml {

QmlVideoEngineProxy::QmlVideoEngineProxy(QObject* parent)
        : QObject(parent),
          m_pEnabledControl(new ControlProxy(
                  ConfigKey("[VideoEngine]", "enabled"),
                  this,
                  ControlFlag::AllowMissingOrInvalid)) {
    m_pEnabledControl->connectValueChanged(this, &QmlVideoEngineProxy::enabledChanged);
}

QmlVideoEngineProxy::~QmlVideoEngineProxy() = default;

QmlVideoEngineProxy* QmlVideoEngineProxy::create(
        QQmlEngine* pQmlEngine, [[maybe_unused]] QJSEngine* pJsEngine) {
    return new QmlVideoEngineProxy(pQmlEngine);
}

bool QmlVideoEngineProxy::isAvailable() const {
    return m_pEnabledControl->valid();
}

bool QmlVideoEngineProxy::isEnabled() const {
    return m_pEnabledControl->toBool();
}

void QmlVideoEngineProxy::setEnabled(bool enabled) {
    m_pEnabledControl->set(enabled ? 1.0 : 0.0);
}

bool QmlVideoEngineProxy::loadVideo(const QString& deckGroup, const QString& filePath) {
    if (!s_pVideoEngineManager) {
        return false;
    }
    return s_pVideoEngineManager->loadVideo(deckGroup, filePath);
}

bool QmlVideoEngineProxy::setCameraSource(const QString& deckGroup, bool enabled) {
    if (!s_pVideoEngineManager) {
        return false;
    }
    return s_pVideoEngineManager->setCameraSource(deckGroup, enabled);
}

bool QmlVideoEngineProxy::enableNdiOutput(bool enabled, const QString& sourceName) {
#ifdef __VIDEO_ENGINE_NDI_OUTPUT__
    if (!s_pVideoEngineManager) {
        return false;
    }
    return s_pVideoEngineManager->enableNdiOutput(enabled, sourceName);
#else
    Q_UNUSED(enabled);
    Q_UNUSED(sourceName);
    return false;
#endif
}

} // namespace qml
} // namespace mixxx
