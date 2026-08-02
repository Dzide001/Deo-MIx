#pragma once

#include <QList>
#include <QObject>

class Controller;

/// Base class handling discovery and enumeration of DJ controllers.
///
/// This class handles discovery and enumeration of DJ controllers and
/// must be inherited by a class that implements it on some API.
class ControllerEnumerator : public QObject {
    Q_OBJECT
  public:
    ControllerEnumerator();
    // In this function, the inheriting class must delete the Controllers it
    // creates
    virtual ~ControllerEnumerator();

    virtual QList<Controller*> queryDevices() = 0;

  protected:
    /// Deferred replacement for `delete` on Controllers dropped by a
    /// queryDevices() re-enumeration: closes the controller immediately
    /// (on this thread, so backend teardown ordering inside
    /// queryDevices() is unchanged), but defers the actual object
    /// deletion by a grace period. Rationale: queryDevices() runs on the
    /// ControllerManager thread, while GUI listeners (DlgPrefControllers,
    /// QML proxies) rebuild their views of these Controller pointers via
    /// a QUEUED devicesChanged() connection on the main thread -- so
    /// deleting immediately leaves a window where the main thread
    /// dereferences a freed Controller. A real crash exactly matching
    /// this (automatic hotplug rescan while the preferences dialog was
    /// open) was hit during manual testing. The grace period is orders
    /// of magnitude longer than the queued-signal latency it papers
    /// over.
    void retireDevice(Controller* pController);

  private:
    QList<Controller*> m_retiredDevices;
};
