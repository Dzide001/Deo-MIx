#pragma once

#include <QImage>
#include <QString>

namespace mixxx {

/// M12 Stage 6: sends VideoEngineManager's composited preview frame out
/// over NDI (docs.ndi.video) so external tools (OBS, vMix, a video wall)
/// can pick it up as a network video source, in addition to the in-app
/// preview window. Only compiled in when built with
/// -DVIDEO_ENGINE_NDI_OUTPUT=ON and -DNDI_ROOT=<path to the NDI SDK> (see
/// the root CMakeLists.txt's VIDEO_ENGINE_NDI_OUTPUT block for why: the SDK
/// is a free-to-use but proprietary, separately-downloaded dependency,
/// same shape as VIDEO_ENGINE_GSTREAMER_ROOT/AI_STEM_SEPARATION's
/// onnxruntime/Eigen3 roots).
///
/// Deliberately NOT a GStreamer element/pipeline branch -- since Stage 4's
/// two-pipeline rewrite, the composited frame only exists as a transient
/// QImage (VideoEngineManager::grabPreviewFrame()'s return value), not a
/// GStreamer buffer anywhere; this class is fed that QImage directly and
/// calls the NDI SDK's own raw frame-send API, with no GStreamer
/// involvement at all (a GStreamer NDI plugin, gst-plugin-ndi, exists but
/// is a Rust/Cargo build with documented arm64-macOS build-maturity
/// issues -- a mismatch with this project's plain-C/C++ static-plugin
/// convention, and there'd be no GStreamer pipeline segment to attach it
/// to here regardless).
///
/// `NDIlib_send_instance_t` (an opaque `void*` in the SDK's own headers) is
/// stored here as a plain `void*` so this header doesn't need to include
/// `<Processing.NDI.Lib.h>` at all, matching the opaque-pointer pattern
/// `videoenginemanager.h` already uses for `GstElement`/`GstPad`.
class NdiOutputSender {
  public:
    NdiOutputSender();
    ~NdiOutputSender();

    /// Starts advertising an NDI source named `sourceName` on the network.
    /// Safe to call again with a different name while already started
    /// (tears down and recreates the sender). Returns false if the NDI
    /// runtime isn't available on this machine (e.g. not installed) or
    /// send-instance creation otherwise fails.
    bool start(const QString& sourceName);

    /// Stops advertising and releases the NDI send instance. Safe to call
    /// when not started.
    void stop();

    bool isRunning() const {
        return m_pSendInstance != nullptr;
    }

    /// Sends one composited frame. No-op if not started (isRunning() ==
    /// false) or if `frame` is null. Internally converts to whatever pixel
    /// format the NDI send API actually wants (32-bit BGRX) -- the caller
    /// can pass any QImage format VideoEngineManager actually produces
    /// (currently always Format_RGB888).
    void pushFrame(const QImage& frame);

  private:
    void* m_pSendInstance; // NDIlib_send_instance_t, kept opaque -- see class comment
    bool m_ndiInitialized;
};

} // namespace mixxx
