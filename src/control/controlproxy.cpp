#include "control/controlproxy.h"

#include "control/control.h"
#include "control/controlobjectscript.h"
#include "moc_controlproxy.cpp"

namespace {
/// M8: ControlObjectScript is the wrapper class controller mapping scripts'
/// engine.setValue()/setParameter() calls actually go through today (see
/// ControllerScriptInterfaceLegacy::setValue()/setParameter()) -- checking
/// pSetter's dynamic type against it, rather than adding a new marker to
/// the generic ControlObject::set() path used by every other caller
/// (engine internals, every QML widget, etc.), keeps this detection scoped
/// to the one real "driven by hardware" write path without touching that
/// hot, shared code.
inline bool isFromControllerScript(QObject* pSetter) {
    return qobject_cast<ControlObjectScript*>(pSetter) != nullptr;
}
} // namespace

ControlProxy::ControlProxy(const QString& g, const QString& i, QObject* pParent, ControlFlags flags)
        : ControlProxy(ConfigKey(g, i), pParent, flags) {
}

ControlProxy::ControlProxy(const ConfigKey& key, QObject* pParent, ControlFlags flags)
        : QObject(pParent) {
    m_pControl = ControlDoublePrivate::getControl(key, flags);
    if (!m_pControl) {
        DEBUG_ASSERT(flags & ControlFlag::AllowMissingOrInvalid);
        m_pControl = ControlDoublePrivate::getDefaultControl();
    }
    DEBUG_ASSERT(m_pControl);
}

ControlProxy::~ControlProxy() {
    //qDebug() << "ControlProxy::~ControlProxy()";
}

const ConfigKey& ControlProxy::getKey() const {
    return m_pControl->getKey();
}

void ControlProxy::slotValueChangedDirect(double v, QObject* pSetter) {
    if (pSetter != this) {
        emit valueChanged(v);
        if (isFromControllerScript(pSetter)) {
            emit valueChangedFromHardware(v);
        }
    }
}

void ControlProxy::slotValueChangedAuto(double v, QObject* pSetter) {
    if (pSetter != this) {
        emit valueChanged(v);
        if (isFromControllerScript(pSetter)) {
            emit valueChangedFromHardware(v);
        }
    }
}

void ControlProxy::slotValueChangedQueued(double v, QObject* pSetter) {
    if (pSetter != this) {
        emit valueChanged(v);
        if (isFromControllerScript(pSetter)) {
            emit valueChangedFromHardware(v);
        }
    }
}
