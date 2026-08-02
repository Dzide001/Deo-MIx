#include "library/videoengine/ndioutputsender.h"

#include <Processing.NDI.Lib.h>

#include "util/logger.h"

namespace {
const mixxx::Logger kLogger("NdiOutputSender");
} // namespace

namespace mixxx {

NdiOutputSender::NdiOutputSender()
        : m_pSendInstance(nullptr), m_ndiInitialized(false) {
    // NDIlib_initialize() is documented as safe to call multiple times
    // across a process (internally reference-counted by the SDK), but each
    // successful call must be matched by NDIlib_destroy() -- tracked here
    // per-instance so this class's own lifetime is self-contained rather
    // than relying on exactly-once global init/teardown living elsewhere.
    m_ndiInitialized = NDIlib_initialize();
    if (!m_ndiInitialized) {
        kLogger.warning()
                << "NDIlib_initialize() failed -- NDI runtime not available on this machine";
    }
}

NdiOutputSender::~NdiOutputSender() {
    stop();
    if (m_ndiInitialized) {
        NDIlib_destroy();
    }
}

bool NdiOutputSender::start(const QString& sourceName) {
    stop(); // safe to call again with a new name -- tear down and recreate
    if (!m_ndiInitialized) {
        return false;
    }
    // p_ndi_name is not copied by NDIlib_send_create -- keep the UTF-8
    // buffer alive for the duration of that call (it is not retained by
    // the SDK afterward, only read during creation).
    QByteArray sourceNameUtf8 = sourceName.toUtf8();
    NDIlib_send_create_t sendDesc;
    sendDesc.p_ndi_name = sourceNameUtf8.constData();
    sendDesc.p_groups = nullptr;
    // VideoEngineManager's own pipelines already pace playback in real
    // time (sync=true appsinks); don't ask the NDI SDK to pace/clock
    // sending as well, which would just be a second, redundant throttle.
    sendDesc.clock_video = false;
    sendDesc.clock_audio = false;
    m_pSendInstance = NDIlib_send_create(&sendDesc);
    if (!m_pSendInstance) {
        kLogger.warning() << "NDIlib_send_create() failed for source" << sourceName;
        return false;
    }
    kLogger.info() << "NDI output started, source name:" << sourceName;
    return true;
}

void NdiOutputSender::stop() {
    if (m_pSendInstance) {
        NDIlib_send_destroy(static_cast<NDIlib_send_instance_t>(m_pSendInstance));
        m_pSendInstance = nullptr;
    }
}

void NdiOutputSender::pushFrame(const QImage& frame) {
    if (!m_pSendInstance || frame.isNull()) {
        return;
    }
    // NDI's send API works in 32-bits-per-pixel formats (BGRA/BGRX/RGBA/
    // RGBX), not VideoEngineManager's tightly-packed 24-bit
    // Format_RGB888 -- convert once per frame rather than assume the
    // caller's format matches. QImage::Format_RGB32 is stored, in memory,
    // as [B, G, R, 0xFF] per pixel on this (little-endian) platform, which
    // is exactly NDIlib_FourCC_video_type_BGRX's byte order, so this is a
    // straight memory-layout match, not a channel-reordering conversion.
    QImage bgrxFrame = frame.convertToFormat(QImage::Format_RGB32);
    if (bgrxFrame.isNull()) {
        return;
    }

    NDIlib_video_frame_v2_t videoFrame;
    videoFrame.xres = bgrxFrame.width();
    videoFrame.yres = bgrxFrame.height();
    videoFrame.FourCC = NDIlib_FourCC_video_type_BGRX;
    // Matches QmlVideoPreviewItem's own ~30fps (33ms) poll rate, which is
    // what actually paces how often pushFrame() is called -- an
    // approximation for the frame-rate metadata NDI receivers display, not
    // a hard real-time guarantee this class enforces itself.
    videoFrame.frame_rate_N = 30;
    videoFrame.frame_rate_D = 1;
    videoFrame.picture_aspect_ratio =
            static_cast<float>(bgrxFrame.width()) / static_cast<float>(bgrxFrame.height());
    videoFrame.frame_format_type = NDIlib_frame_format_type_progressive;
    videoFrame.timecode = NDIlib_send_timecode_synthesize;
    videoFrame.p_data = const_cast<uint8_t*>(bgrxFrame.constBits());
    videoFrame.line_stride_in_bytes = bgrxFrame.bytesPerLine();
    videoFrame.p_metadata = nullptr;
    videoFrame.timestamp = 0;

    // Synchronous send -- NDIlib_send_send_video_v2 blocks until the SDK
    // has captured the frame internally (not until a receiver has consumed
    // it). Acceptable here without its own worker thread: the sole caller
    // (VideoEngineManager::grabPreviewFrame()) already runs on
    // QmlVideoPreviewItem's ~33ms poll timer, not a latency-critical path,
    // the same reasoning that already applies to that timer's own
    // non-blocking design elsewhere in this feature.
    NDIlib_send_send_video_v2(static_cast<NDIlib_send_instance_t>(m_pSendInstance), &videoFrame);
}

} // namespace mixxx
