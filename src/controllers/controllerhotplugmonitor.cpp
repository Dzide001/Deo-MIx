#include "controllers/controllerhotplugmonitor.h"

#include <QtDebug>

#include "moc_controllerhotplugmonitor.cpp"

namespace {
constexpr int kDebounceMs = 300;
constexpr int kRunLoopPumpMs = 250;

#ifdef Q_OS_MACOS
void addUsageMatch(CFMutableArrayRef matchArray, int usagePage, int usage) {
    CFNumberRef pageRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usagePage);
    CFNumberRef usageRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
            0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsagePageKey), pageRef);
    CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsageKey), usageRef);
    CFArrayAppendValue(matchArray, dict);
    CFRelease(pageRef);
    CFRelease(usageRef);
    CFRelease(dict);
}

// Matches every usage under a given usage page, regardless of usage. Used
// for the vendor-defined page sweep, since a vendor-defined device's usage
// value within its own page isn't standardized.
void addPageMatch(CFMutableArrayRef matchArray, int usagePage) {
    CFNumberRef pageRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usagePage);
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
            0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsagePageKey), pageRef);
    CFArrayAppendValue(matchArray, dict);
    CFRelease(pageRef);
    CFRelease(dict);
}
#endif
} // namespace

ControllerHotplugMonitor::ControllerHotplugMonitor(QObject* pParent)
        : QObject(pParent) {
    m_debounceTimer.setSingleShot(true);
    connect(&m_debounceTimer,
            &QTimer::timeout,
            this,
            &ControllerHotplugMonitor::devicesMayHaveChanged);
#ifdef Q_OS_MACOS
    // See m_runLoopPumpTimer's declaration comment: drains this worker
    // thread's otherwise-never-run CFRunLoop so the native CoreMIDI/
    // IOHIDManager callbacks registered in start() actually get
    // delivered. Zero-second, source-handled-limited passes keep each
    // tick cheap and non-blocking.
    connect(&m_runLoopPumpTimer, &QTimer::timeout, this, [] {
        while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) ==
                kCFRunLoopRunHandledSource) {
        }
    });
#endif
}

ControllerHotplugMonitor::~ControllerHotplugMonitor() {
    stop();
}

#ifdef Q_OS_MACOS

void ControllerHotplugMonitor::scheduleDebouncedSignal() {
    // QTimer::start() restarts an already-running timer, so a burst of
    // native callbacks (e.g. several devices on a hub attaching at once)
    // coalesces into a single devicesMayHaveChanged() emission kDebounceMs
    // after the LAST callback, not the first.
    m_debounceTimer.start(kDebounceMs);
}

void ControllerHotplugMonitor::midiNotifyCallback(
        const MIDINotification* pMessage, void* pRefCon) {
    Q_UNUSED(pMessage);
    static_cast<ControllerHotplugMonitor*>(pRefCon)->scheduleDebouncedSignal();
}

void ControllerHotplugMonitor::hidDeviceCallback(
        void* pContext, IOReturn result, void* pSender, IOHIDDeviceRef device) {
    Q_UNUSED(result);
    Q_UNUSED(pSender);
    Q_UNUSED(device);
    static_cast<ControllerHotplugMonitor*>(pContext)->scheduleDebouncedSignal();
}

// static
void ControllerHotplugMonitor::collectDeviceUsageMatch(const void* pValue, void* pContext) {
    IOHIDDeviceRef device = static_cast<IOHIDDeviceRef>(const_cast<void*>(pValue));
    if (IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) ||
            IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse) ||
            IOHIDDeviceConformsTo(device, kHIDPage_GenericDesktop, kHIDUsage_GD_Pointer)) {
        return;
    }
    CFNumberRef pageProp = static_cast<CFNumberRef>(
            IOHIDDeviceGetProperty(device, CFSTR(kIOHIDPrimaryUsagePageKey)));
    CFNumberRef usageProp = static_cast<CFNumberRef>(
            IOHIDDeviceGetProperty(device, CFSTR(kIOHIDPrimaryUsageKey)));
    int page = 0;
    int usage = 0;
    if (pageProp != nullptr && usageProp != nullptr &&
            CFNumberGetValue(pageProp, kCFNumberIntType, &page) &&
            CFNumberGetValue(usageProp, kCFNumberIntType, &usage)) {
        addUsageMatch(static_cast<CFMutableArrayRef>(pContext), page, usage);
    }
}

void ControllerHotplugMonitor::start() {
    if (!m_runLoopPumpTimer.isActive()) {
        m_runLoopPumpTimer.start(kRunLoopPumpMs);
    }

#ifdef __PORTMIDI__
    if (m_midiClient == 0) {
        OSStatus status = MIDIClientCreate(CFSTR("Mixxx Hotplug Monitor"),
                &ControllerHotplugMonitor::midiNotifyCallback,
                this,
                &m_midiClient);
        if (status != noErr) {
            qWarning() << "ControllerHotplugMonitor: MIDIClientCreate failed:" << status;
            m_midiClient = 0;
        }
    }
#endif

#ifdef __HID__
    if (m_hidManager == nullptr) {
        IOHIDManagerRef hidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
        if (hidManager != nullptr) {
            // IOHIDManagerOpen() below establishes a live I/O connection to
            // every device the manager's matching criteria selects. If that
            // includes the built-in keyboard and trackpad (i.e. matching
            // *all* devices via IOHIDManagerSetDeviceMatching(hidManager,
            // nullptr)), Open() fails wholesale with kIOReturnExclusiveAccess
            // (-536870203): macOS's WindowServer/IOHIDSystem already holds
            // those built-in devices open exclusively for secure input, and
            // IOKit doesn't allow a second exclusive opener. Confirmed
            // empirically (this is not a TCC/Input Monitoring permission
            // issue -- a signed, hardened-runtime bundle with a stable
            // identity still failed identically, and never even appeared in
            // System Settings > Privacy & Security > Input Monitoring).
            //
            // The fix is to keep the built-in keyboard/mouse/pointer out of
            // the matching set entirely, so Open() never tries to touch
            // them, while still matching literally everything else a
            // future controller could plug in as:
            //   - the standard "game controller" Generic Desktop usages
            //   - every vendor-defined usage page (0xFF00-0xFFFF) as a
            //     whole -- real DJ controllers commonly describe their
            //     entire top-level HID collection under a proprietary
            //     vendor-defined usage rather than Joystick/GamePad, and
            //     since the point of this matching set is to catch
            //     devices that are NOT yet connected when start() runs,
            //     it can't be limited to only usages seen on already-
            //     connected hardware.
            //   - the exact usage page/usage of every already-connected
            //     non-keyboard/mouse/pointer device too, as a cheap extra
            //     safety net for anything outside both sets above (e.g. a
            //     Consumer-page device).
            // IOHIDManager matching dictionaries are OR'd together with no
            // "exclude" primitive, so allow-listing "everything except a
            // few specific usages" is the only way to express this.
            CFMutableArrayRef matchArray =
                    CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            addUsageMatch(matchArray, kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
            addUsageMatch(matchArray, kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);
            addUsageMatch(matchArray, kHIDPage_GenericDesktop, kHIDUsage_GD_MultiAxisController);
            for (int page = kHIDPage_VendorDefinedStart; page <= 0xFFFF; page++) {
                addPageMatch(matchArray, page);
            }

            IOHIDManagerRef probeManager =
                    IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
            if (probeManager != nullptr) {
                IOHIDManagerSetDeviceMatching(probeManager, nullptr);
                CFSetRef devices = IOHIDManagerCopyDevices(probeManager);
                if (devices != nullptr) {
                    CFSetApplyFunction(devices,
                            &ControllerHotplugMonitor::collectDeviceUsageMatch,
                            matchArray);
                    CFRelease(devices);
                }
                CFRelease(probeManager);
            }

            IOHIDManagerSetDeviceMatchingMultiple(hidManager, matchArray);
            CFRelease(matchArray);

            IOHIDManagerRegisterDeviceMatchingCallback(
                    hidManager, &ControllerHotplugMonitor::hidDeviceCallback, this);
            IOHIDManagerRegisterDeviceRemovalCallback(
                    hidManager, &ControllerHotplugMonitor::hidDeviceCallback, this);
            IOHIDManagerScheduleWithRunLoop(
                    hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
            // IOHIDManagerOpen() IS required for the matching callback to
            // keep firing for devices that connect *after* registration --
            // confirmed empirically: without it, only the one-time
            // "already connected at registration" burst is delivered, and
            // genuine live plug events never arrive.
            IOReturn openResult = IOHIDManagerOpen(hidManager, kIOHIDOptionsTypeNone);
            if (openResult == kIOReturnSuccess) {
                m_hidManager = hidManager;
            } else {
                qWarning() << "ControllerHotplugMonitor: IOHIDManagerOpen failed:" << openResult
                           << "-- HID hotplug detection unavailable. MIDI hotplug is "
                              "unaffected.";
                IOHIDManagerUnscheduleFromRunLoop(
                        hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
                CFRelease(hidManager);
            }
        } else {
            qWarning() << "ControllerHotplugMonitor: IOHIDManagerCreate failed";
        }
    }
#endif
}

void ControllerHotplugMonitor::stop() {
    m_debounceTimer.stop();
    m_runLoopPumpTimer.stop();
    if (m_midiClient != 0) {
        MIDIClientDispose(m_midiClient);
        m_midiClient = 0;
    }
    if (m_hidManager != nullptr) {
        IOHIDManagerUnscheduleFromRunLoop(m_hidManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        IOHIDManagerClose(m_hidManager, kIOHIDOptionsTypeNone);
        CFRelease(m_hidManager);
        m_hidManager = nullptr;
    }
}

#else // !Q_OS_MACOS

void ControllerHotplugMonitor::start() {
    // Not implemented on this platform for this macOS-focused fork; the
    // manual Rescan button remains the only option elsewhere.
}

void ControllerHotplugMonitor::stop() {
}

#endif // Q_OS_MACOS
