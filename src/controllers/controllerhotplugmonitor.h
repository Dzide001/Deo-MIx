#pragma once

#include <QObject>
#include <QTimer>
#include <QtGlobal>

#ifdef Q_OS_MACOS
#include <CoreMIDI/CoreMIDI.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#endif

/// Watches for OS-level MIDI/HID controller plug and unplug events on macOS
/// (CoreMIDI's MIDIClientCreate notification callback, and IOKit's
/// IOHIDManager device matching/removal callbacks), and emits a debounced
/// signal so ControllerManager can trigger an automatic rescan without the
/// user clicking the manual "Rescan" button. No-op stub on every other
/// platform -- out of scope for this macOS-focused fork.
///
/// Must be constructed and start()ed only on ControllerManager's own
/// dedicated thread (from the end of slotSetUpDevices()) -- both native
/// APIs schedule notification delivery on "the current run loop" at
/// registration time. CAUTION: Qt does NOT integrate a worker QThread's
/// event loop with its CFRunLoop on macOS (only the GUI thread gets the
/// CoreFoundation event dispatcher), so that run loop never runs by
/// itself and neither native callback would ever be delivered; see
/// m_runLoopPumpTimer for the periodic drain that makes delivery work.
/// Both native callbacks then fire on this same thread from inside that
/// drain, so there is no cross-thread hazard between the callbacks and
/// this object; stop()/the destructor just need to run before the
/// thread's event loop is torn down (see
/// ControllerManager::slotShutdown()).
class ControllerHotplugMonitor : public QObject {
    Q_OBJECT
  public:
    explicit ControllerHotplugMonitor(QObject* pParent = nullptr);
    ~ControllerHotplugMonitor() override;

    /// Installs native OS hotplug callbacks for whichever controller
    /// backends are compiled in (__PORTMIDI__ / __HID__). No-op on
    /// non-macOS platforms.
    void start();
    /// Uninstalls native OS hotplug callbacks. Idempotent: safe to call
    /// multiple times, safe even if start() was never called or partially
    /// failed.
    void stop();

  signals:
    /// Emitted at most once per kDebounceMs quiet period after one or more
    /// native hotplug notifications fire. Carries no information about
    /// what changed -- receivers just re-enumerate, exactly like the
    /// manual Rescan button does.
    void devicesMayHaveChanged();

  private:
#ifdef Q_OS_MACOS
    static void midiNotifyCallback(const MIDINotification* pMessage, void* pRefCon);
    static void hidDeviceCallback(
            void* pContext, IOReturn result, void* pSender, IOHIDDeviceRef device);
    /// CFSetApplyFunction callback used while building the HID matching
    /// set in start(): appends the (usage page, usage) pair of each
    /// already-connected device to the CFMutableArrayRef passed as
    /// pContext, skipping Keyboard/Mouse/Pointer devices. See start()'s
    /// comment for why this exists.
    static void collectDeviceUsageMatch(const void* pValue, void* pContext);
    void scheduleDebouncedSignal();

    MIDIClientRef m_midiClient = 0;
    IOHIDManagerRef m_hidManager = nullptr;
    /// Periodically drains this thread's CFRunLoop. Required because this
    /// object lives on ControllerManager's worker QThread, and Qt worker
    /// threads on macOS use a poll-based event dispatcher -- their
    /// CFRunLoop is NEVER pumped by Qt. Both CoreMIDI notifications
    /// (which macOS delivers to the run loop that was current at the
    /// process's FIRST MIDIClientCreate call -- PortMidi's, made on this
    /// same thread) and the IOHIDManager callbacks (scheduled on this
    /// thread's run loop in start()) sit undelivered forever without
    /// this. Confirmed empirically: with a standalone test tool the OS
    /// delivers both notification types fine, while inside Mixxx neither
    /// callback ever fired until this pump was added.
    QTimer m_runLoopPumpTimer;
#endif
    QTimer m_debounceTimer;
};
