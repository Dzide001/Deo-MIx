#include "controllers/controllerenumerator.h"

#include <QTimer>

#include "controllers/controller.h"
#include "moc_controllerenumerator.cpp"

namespace {
// See retireDevice()'s header comment: must comfortably exceed the
// worst-case latency of the queued devicesChanged() delivery plus the
// GUI's rebuild of its controller views (milliseconds in practice).
constexpr int kRetiredControllerGraceMs = 15000;
} // namespace

ControllerEnumerator::ControllerEnumerator() = default;

ControllerEnumerator::~ControllerEnumerator() {
    // Pending single-shot timers targeting `this` die with the QObject,
    // so any not-yet-purged retirees must be deleted here instead.
    qDeleteAll(m_retiredDevices);
    m_retiredDevices.clear();
}

void ControllerEnumerator::retireDevice(Controller* pController) {
    if (pController == nullptr) {
        return;
    }
    if (pController->isOpen()) {
        pController->close();
    }
    m_retiredDevices.append(pController);
    QTimer::singleShot(kRetiredControllerGraceMs, this, [this, pController]() {
        if (m_retiredDevices.removeOne(pController)) {
            delete pController;
        }
    });
}
